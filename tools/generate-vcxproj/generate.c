#include <stdio.h>
#include <stdint.h>
#include "tomlc99/toml.h"
#include "bearssl_wrapper.h"
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#ifdef _MSC_VER
#define strcasecmp(a,b) _stricmp(a,b)
#endif

#define fprint(f,str) fputs(str,f)
#define UUID_LEN 40

typedef struct project project;
typedef struct target target;
typedef struct command command;
typedef struct string_list string_list;

struct string_list {
	struct string_list *next;
	char *str;
};

struct project {
	struct project *next;
	char *name;
	char *file;
	char *target;
	char uuid[UUID_LEN];
	string_list *dirs;
};

struct command {
	const char *name;
	const char *build;
	const char *clean;
	const char *rebuild;
	char uuid[UUID_LEN];
};

struct target {
	struct target *next;
	char *vs;
	char *ninja;
	char *default_target;
	char *defines;
};

static FILE *must_fopen(const char *name, const char *mode) {
	FILE *f = fopen(name, mode);
	if (f == NULL) {
		fprintf(stderr, "failed to open %s\n", name);
		exit(3);
	}
	return f;
}

static char *get_string(toml_table_t *t, const char *key, const char *defstr) {
	const char *val = toml_raw_in(t, key);
	char *ret;
	if (toml_rtos(val, &ret)) {
		return strdup(defstr);
	}
	return ret;
}

static char *must_get_string(toml_table_t *t, const char *key) {
	char *val = get_string(t, key, NULL);
	if (!val) {
		fprintf(stderr, "missing required %s field\n", key);
		exit(3);
	}
	return val;
}

static string_list *get_string_list(toml_table_t *t, const char *key) {
	toml_array_t *array = toml_array_in(t, key);
	if (!array) {
		return NULL;
	}
	string_list *list = NULL;
	string_list **pnext = &list;
	int idx = 0;
	const char *raw;
	while ((raw = toml_raw_at(array, idx++)) != NULL) {
		char *val;
		if (toml_rtos(raw, &val)) {
			fprintf(stderr, "invalid string in list\n");
			exit(4);
		}
		string_list *item = (string_list*)malloc(sizeof(string_list));
		item->str = val;
		item->next = NULL;
		*pnext = item;
		pnext = &item->next;
	}
	return list;
}

static const char *extension(const char *file) {
	const char *ext = strrchr(file, '.');
	return ext ? ext : "";
}

static void print_cfile(FILE *f, const char *dir, const char *file) {
	const char *ext = extension(file);
	if (!strcasecmp(ext, ".c")
		|| !strcasecmp(ext, ".cpp")
		|| !strcasecmp(ext, ".s")
		|| !strcasecmp(ext, ".asm")) {
		fprintf(f, "    <ClCompile Include=\"%s\\%s\" />\r\n", dir, file);
	}
}

static void print_hfile(FILE *f, const char *dir, const char *file) {
	const char *ext = extension(file);
	if (!strcasecmp(ext, ".h")
		|| !strcasecmp(ext, ".hpp")) {
		fprintf(f, "    <ClInclude Include=\"%s\\%s\" />\r\n", dir, file);
	}
}

static void print_other(FILE *f, const char *dir, const char *file) {
	const char *ext = extension(file);
	if (!strcasecmp(ext, ".proto")
		|| !strcasecmp(ext, ".txt")
		|| !strcasecmp(ext, ".css")
		|| !strcasecmp(ext, ".js")
		|| !strcasecmp(ext, ".html")
		|| !strcasecmp(ext, ".sh")
		|| !strcasecmp(ext, ".json")) {
		fprintf(f, "    <None Include=\"%s\\%s\" />\r\n", dir, file);
	}
}

typedef void(*print_fn)(FILE*, const char *dir, const char *file);

#ifdef _WIN32
static void list_directory(FILE *f, print_fn fn, const char *dir) {
	char *str = malloc(strlen(dir) + 2 + 1);
	strcpy(str, dir);
	strcat(str, "\\*");
	WIN32_FIND_DATA fd;
	HANDLE h = FindFirstFileA(str, &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) {
				fn(f, dir, fd.cFileName);
			}
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
}
#else
static void list_directory(FILE *f, print_fn fn, const char *dir) {
	DIR *dirp = opendir(dir);
	struct dirent *d;
	while ((d = readdir(dirp)) != NULL) {
		if (d->d_type == DT_REG) {
			fn(f, dir, d->d_name);
		}
	}
	closedir(dirp);
}
#endif

static void generate_uuid(char *buf, const char *file) {
	static const unsigned char nsuuid[] = {
		0x7D, 0x2B, 0x2A, 0x65,
		0x3F, 0x9E,
		0x4E, 0xC4,
		0xB4, 0x5B,
		0xE1, 0xE1, 0xF0, 0x89, 0x2E, 0xAC,
	};
	br_sha1_context ctx;
	br_sha1_init(&ctx);
	br_sha1_update(&ctx, nsuuid, sizeof(nsuuid));
	br_sha1_update(&ctx, file, strlen(file));

	unsigned char h[br_sha1_SIZE];
	br_sha1_out(&ctx, h);
	h[6] = (h[6] & 0xF) | 0x50; // set version
	h[8] = (h[8] & 0x3F) | 0x80; // set variant
	snprintf(buf, UUID_LEN, "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		h[0], h[1], h[2], h[3],
		h[4], h[5],
		h[6], h[7],
		h[8], h[9],
		h[10], h[11], h[12], h[13], h[14], h[15]);
}

static void change_to_backslash(char *p) {
	while (*p) {
		if (*p == '/') {
			*p = '\\';
		}
		p++;
	}
}

static char *dup_with_replacement(const char *src, const char *test, const char *replacement) {
	const char *found = strstr(src, test);
	if (!found) {
		return strdup(src);
	}

	const char *tail = found + strlen(test);
	size_t sz = (found - src) + strlen(replacement) + strlen(tail);
	char *ret = (char*) malloc(sz + 1);
	memcpy(ret, src, found - src);
	strcpy(ret + (found - src), replacement);
	strcat(ret + (found - src), tail);
	return ret;
}

static void write_project(FILE *f, project *p, target *tgts, string_list *includes, const char *argv0) {
	fprint(f, "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
		"<Project DefaultTargets=\"Build\" ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n"
		"  <ItemGroup Label=\"ProjectConfigurations\">\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {
		fprintf(f, "    <ProjectConfiguration Include=\"%s|Win32\">\r\n", t->vs);
		fprintf(f, "      <Configuration>%s</Configuration>\r\n", t->vs);
		fprint(f, "      <Platform>Win32</Platform>\r\n"
			"    </ProjectConfiguration>\r\n");
	}

	fprint(f, "  </ItemGroup>\r\n"
	          "  <ItemGroup>\r\n");
	for (string_list *s = p->dirs; s != NULL; s = s->next) {
		list_directory(f, &print_cfile, s->str);
	}
	fprint(f, "  </ItemGroup>\r\n"
	          "  <ItemGroup>\r\n");
	for (string_list *s = p->dirs; s != NULL; s = s->next) {
		list_directory(f, &print_hfile, s->str);
	}
	fprint(f, "  </ItemGroup>\r\n"
	          "  <ItemGroup>\r\n");
	for (string_list *s = p->dirs; s != NULL; s = s->next) {
		list_directory(f, &print_other, s->str);
	}
	fprint(f, "  </ItemGroup>\r\n"
		"  <PropertyGroup Label=\"Globals\">\r\n");
	fprintf(f, "    <ProjectGuid>%s</ProjectGuid>\r\n", p->uuid);
	fprint(f, "    <Keyword>MakeFileProj</Keyword>\r\n");
	fprintf(f, "    <ProjectName>%s</ProjectName>\r\n", p->name);
	fprintf(f, "  </PropertyGroup>\r\n"
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {
		fprintf(f, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\" Label=\"Configuration\">\r\n", t->vs);
		fprintf(f, "    <ConfigurationType>Makefile</ConfigurationType>\r\n"
			"    <UseDebugLibraries>false</UseDebugLibraries>\r\n"
			"    <PlatformToolset>v140</PlatformToolset>\r\n"
			"  </PropertyGroup>\r\n");
	}

	fprint(f, "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\r\n"
		"  <ImportGroup Label=\"ExtensionSettings\">\r\n"
		"  </ImportGroup>\r\n"
		"  <ImportGroup Label=\"Shared\">\r\n"
		"  </ImportGroup>\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {
		fprintf(f, "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\">\r\n", t->vs);
		fprint(f, "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\r\n"
			"  </ImportGroup>\r\n");
	}

	fprintf(f, "  <PropertyGroup Label=\"UserMacros\" />\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {
		char *njtgt = dup_with_replacement(p->target, "{TGT}", t->ninja);
		char *vstgt = strdup(njtgt);
		change_to_backslash(vstgt);

		fprintf(f, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\">\r\n", t->vs);
		fprintf(f, "    <NMakeOutput>$(SolutionDir)\\%s</NMakeOutput>\r\n", vstgt);
		fprintf(f, "    <NMakePreprocessorDefinitions>%s</NMakePreprocessorDefinitions>\r\n", t->defines);
		fprintf(f, "    <NMakeBuildCommandLine>%s &amp;&amp; ninja.exe -C $(SolutionDir) -f msvc.ninja %s</NMakeBuildCommandLine>\r\n", argv0, njtgt);
		fprintf(f, "    <NMakeReBuildCommandLine>%s &amp;&amp; ninja.exe -C $(SolutionDir) -f msvc.ninja -t clean %s &amp;&amp; ninja.exe -C $(SolutionDir) -f msvc.ninja %s</NMakeReBuildCommandLine>\r\n", argv0, njtgt, njtgt);
		fprintf(f, "    <NMakeCleanCommandLine>%s &amp;&amp; ninja.exe -C $(SolutionDir) -f msvc.ninja -t clean %s</NMakeCleanCommandLine>\r\n", argv0, njtgt);

		fprint(f, "    <NMakeIncludeSearchPath>$(ProjectDir)");
		for (string_list *inc = includes; inc != NULL; inc = inc->next) {
			fprintf(f, ";$(SolutionDir)\\%s", inc->str);
		}
		fprintf(f, "</NMakeIncludeSearchPath>\r\n"
			"    <IntDir>$(SolutionDir)\\obj\\$(Configuration)\\</IntDir>\r\n"
			"    <SourcePath />\r\n"
			"    <ExcludePath />\r\n"
			"  </PropertyGroup>\r\n");

		free(vstgt);
		free(njtgt);
	}

	fprintf(f, "  <ItemDefinitionGroup>\r\n"
		"  </ItemDefinitionGroup>\r\n"
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\r\n"
		"  <ImportGroup Label=\"ExtensionTargets\">\r\n"
		"  </ImportGroup>\r\n"
		"</Project>\r\n");
}

static void write_command(FILE *f, command *c, target *targets) {
	fprintf(f, "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
		"<Project DefaultTargets=\"Build\" ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n"
		"  <ItemGroup Label=\"ProjectConfigurations\">\r\n");

	for (target *t = targets; t != NULL; t = t->next) {
		fprintf(f, "    <ProjectConfiguration Include=\"%s|Win32\">\r\n", t->vs);
		fprintf(f, "      <Configuration>%s</Configuration>\r\n", t->vs);
		fprint(f, "      <Platform>Win32</Platform>\r\n"
			"    </ProjectConfiguration>\r\n");
	}

	fprint(f, "  </ItemGroup>\r\n");

	fprint(f, "  <PropertyGroup Label=\"Globals\">\r\n");
	fprintf(f, "    <ProjectGuid>%s</ProjectGuid>\r\n", c->uuid);
	fprint(f, "    <Keyword>MakeFileProj</Keyword>\r\n");
	fprintf(f, "    <ProjectName>%s</ProjectName>\r\n", c->name);
	fprint(f, "  </PropertyGroup>\r\n"
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\r\n");

	for (target *t = targets; t != NULL; t = t->next) {
		fprintf(f, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\" Label=\"Configuration\">\r\n", t->vs);
		fprint(f, "    <ConfigurationType>Makefile</ConfigurationType>\r\n"
			"    <UseDebugLibraries>false</UseDebugLibraries>\r\n"
			"    <PlatformToolset>v140</PlatformToolset>\r\n"
			"  </PropertyGroup>\r\n");
	}

	fprint(f, "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\r\n"
		"  <ImportGroup Label=\"ExtensionSettings\">\r\n"
		"  </ImportGroup>\r\n"
		"  <ImportGroup Label=\"Shared\">\r\n"
		"  </ImportGroup>\r\n");

	for (target *t = targets; t != NULL; t = t->next) {
		fprintf(f, "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\">\r\n", t->vs);
		fprint(f, "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\r\n"
			"  </ImportGroup>\r\n");
	}

	fprintf(f, "  <PropertyGroup Label=\"UserMacros\" />\r\n");

	for (target *t = targets; t != NULL; t = t->next) {
		char *build = dup_with_replacement(c->build, "{DEFAULT}", t->default_target);
		char *rebuild = dup_with_replacement(c->rebuild, "{DEFAULT}", t->default_target);
		char *clean = dup_with_replacement(c->clean, "{DEFAULT}", t->default_target);

		fprintf(f, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\">\r\n", t->vs);
		fprintf(f, "    <NMakeBuildCommandLine>%s</NMakeBuildCommandLine>\r\n", build);
		fprintf(f, "    <NMakeReBuildCommandLine>%s</NMakeReBuildCommandLine>\r\n", rebuild);
		fprintf(f, "    <NMakeCleanCommandLine>%s</NMakeCleanCommandLine>\r\n", clean);
		fprint(f, "  </PropertyGroup>\r\n");

		free(build);
		free(rebuild);
		free(clean);
	}

	fprint(f, "  <ItemDefinitionGroup>\r\n"
		"  </ItemDefinitionGroup>\r\n"
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\r\n"
		"  <ImportGroup Label=\"ExtensionTargets\">\r\n"
		"  </ImportGroup>\r\n"
		"</Project>\r\n");
}

static void write_solution(FILE *f, project *projects, target *targets, command *defcmd, command *gencmd) {
	fprintf(f, "\xEF\xBB\xBF\r\n"
		"Microsoft Visual Studio Solution File, Format Version 12.00\r\n"
		"# Visual Studio 14\r\n"
		"VisualStudioVersion = 14.0.25420.1\r\n"
		"MinimumVisualStudioVersion = 10.0.40219.1\r\n");

	fprintf(f, "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"%s\", \"%s\", \"%s\"\r\nEndProject\r\n", "_DEFAULT", "_DEFAULT.vcxproj", defcmd->uuid);
	fprintf(f, "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"%s\", \"%s\", \"%s\"\r\nEndProject\r\n", "_GENERATE_VCXPROJ", "_GENERATE_VCXPROJ.vcxproj", gencmd->uuid);

	for (project *p = projects; p != NULL; p = p->next) {
		fprintf(f, "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"%s\", \"%s\", \"%s\"\r\nEndProject\r\n", p->name, p->file, p->uuid);
	}

	fprint(f, "Global\r\n\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\r\n");
	for (target *t = targets; t != NULL; t = t->next) {
		fprintf(f, "\t\t%s|ALL = %s|ALL\r\n", t->vs, t->vs);
	}
	fprint(f, "\tEndGlobalSection\r\n"
		"\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\r\n");

	for (project *p = projects; p != NULL; p = p->next) {
		for (target *t = targets; t != NULL; t = t->next) {
			fprintf(f, "\t\t%s.%s|ALL.ActiveCfg = %s|Win32\r\n", p->uuid, t->vs, t->vs);
		}
	}

	for (target *t = targets; t != NULL; t = t->next) {
		fprintf(f, "\t\t%s.%s|ALL.ActiveCfg = %s|Win32\r\n", gencmd->uuid, t->vs, t->vs);
	}

	for (target *t = targets; t != NULL; t = t->next) {
		fprintf(f, "\t\t%s.%s|ALL.ActiveCfg = %s|Win32\r\n", defcmd->uuid, t->vs, t->vs);
		fprintf(f, "\t\t%s.%s|ALL.Build.0 = %s|Win32\r\n", defcmd->uuid, t->vs, t->vs);
	}

	fprint(f, "\tEndGlobalSection\r\n"
		"\tGlobalSection(SolutionProperties) = preSolution\r\n"
		"\t\tHideSolutionNode = FALSE\r\n"
		"\tEndGlobalSection\r\n"
		"EndGlobal\r\n");
}

int main(int argc, char *argv[]) {
	const char *argv0 = argv[0];
	if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		fprintf(stderr, "usage: generate-vcxproj.exe config.toml\n");
		return 2;
	}

	FILE *f = fopen(argv[1], "r");
	if (f == NULL) {
		fprintf(stderr, "failed to open config file %s\n", argv[1]);
		return 3;
	}

	char errbuf[128];
	toml_table_t *root = toml_parse_file(f, errbuf, sizeof(errbuf));
	if (root == NULL) {
		fprintf(stderr, "parse error with config file %s: %s\n", argv[1], errbuf);
		return 4;
	}
	fclose(f);

	toml_table_t *tbl;
	project *projects = NULL;
	project **pproj = &projects;
	int idx = 0;
	toml_array_t *array = toml_array_in(root, "project");
	while (array && (tbl = toml_table_at(array, idx++)) != NULL) {
		project *p = (project*)calloc(1, sizeof(project));
		p->name = must_get_string(tbl, "name");
		p->file = must_get_string(tbl, "file");
		p->target = must_get_string(tbl, "target");
		p->dirs = get_string_list(tbl, "dirs");
		generate_uuid(p->uuid, p->file);
		*pproj = p;
		pproj = &p->next;
	}

	target *targets = NULL;
	target **ptarg = &targets;
	idx = 0;
	array = toml_array_in(root, "target");
	while (array && (tbl = toml_table_at(array, idx++)) != NULL) {
		target *t = (target*)calloc(1, sizeof(target));
		t->vs = must_get_string(tbl, "vs");
		t->ninja = must_get_string(tbl, "ninja");
		t->default_target = must_get_string(tbl, "default");
		t->defines = get_string(tbl, "defines", "");
		*ptarg = t;
		ptarg = &t->next;
	}

	string_list *includes = get_string_list(root, "includes");
	const char *slnfile = must_get_string(root, "solution");

	for (string_list *inc = includes; inc != NULL; inc = inc->next) {
		change_to_backslash(inc->str);
	}


	char build[256], rebuild[256], clean[256];
	snprintf(build, sizeof(build), "%s &amp;&amp; ninja.exe -f msvc.ninja {DEFAULT}", argv0);
	snprintf(rebuild, sizeof(rebuild), "%s &amp;&amp; ninja.exe -f msvc.ninja -t clean {DEFAULT} &amp;&amp; ninja.exe -f msvc.ninja {DEFAULT}", argv0);
	snprintf(clean, sizeof(clean), "%s &amp;&amp; ninja.exe -f msvc.ninja -t clean {DEFAULT}", argv0);

	command def;
	def.name = "_DEFAULT";
	def.build = build;
	def.rebuild = rebuild;
	def.clean = clean;
	generate_uuid(def.uuid, "_DEFAULT.vcxproj");

	command gen;
	gen.name = "_GENERATE_VCXPROJ";
	gen.build = argv0;
	gen.rebuild = argv0;
	gen.clean = "";
	generate_uuid(gen.uuid, "_GENERATE_VCXPROJ.vcxproj");

	f = must_fopen(slnfile, "wb");
	write_solution(f, projects, targets, &def, &gen);
	fclose(f);

	f = must_fopen("_DEFAULT.vcxproj", "wb");
	write_command(f, &def, targets);
	fclose(f);

	f = must_fopen("_GENERATE_VCXPROJ.vcxproj", "wb");
	write_command(f, &gen, targets);
	fclose(f);

	for (project *p = projects; p != NULL; p = p->next) {
		f = must_fopen(p->file, "wb");
		write_project(f, p, targets, includes, argv0);
		fclose(f);
	}

	return 0;
}
