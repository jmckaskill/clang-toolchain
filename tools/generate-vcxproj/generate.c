#include <stdio.h>
#include <stdint.h>
#include "tomlc99/toml.h"
#include "bearssl_wrapper.h"
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#include <direct.h>
#define chdir(x) _chdir(x)
#else
#include <dirent.h>
#include <unistd.h>
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
	string_list *defines;
	string_list *includes;
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
	if (!val) {
		if (toml_table_in(t, key) || toml_array_in(t, key)) {
			fprintf(stderr, "wrong type for %s field - expected string\n", key);
			exit(3);
		}
		return defstr ? strdup(defstr) : NULL;
	}
	char *ret;
	if (toml_rtos(val, &ret)) {
		fprintf(stderr, "malformed %s field - expected string\n", key);
		exit(3);
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
		if (toml_raw_in(t, key) || toml_table_in(t, key)) {
			fprintf(stderr, "wrong type for %s field - expected array\n", key);
			exit(3);
		}
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

static void print_relative_path(FILE *f, const char *vcxfile, const char *dir) {
	// vcxfile is the path to the vcxproj file e.g. a\b\c.vcxproj
	// dir is the directory we want to go to e.g. a\d\e
	// in this case we want to provide the shortest relative path e.g. ..\d\e

	// remove common directories from both paths
	const char *next;
	while ((next = strchr(vcxfile, '\\')) != NULL) {
		size_t flen = next - vcxfile;
		if (strncmp(vcxfile, dir, flen) || (dir[flen] && dir[flen] != '\\')) {
			break;
		}
		dir += dir[flen] ? (flen+1) : flen;
		vcxfile = next + 1;
	}

	// then go up to the common ancestor
	const char *prev = vcxfile;
	while ((next = strchr(prev, '\\')) != NULL) {
		fprint(f, "..\\");
		while (*next == '\\') {
			next++;
		}
		prev = next;
	}

	// then back down into the target directory
	if (*dir) {
		fprintf(f, "%s\\", dir);
	}
}

static void print_cfile(FILE *f, const char *vcxfile, const char *dir, const char *file) {
	const char *ext = extension(file);
	if (!strcasecmp(ext, ".c")
		|| !strcasecmp(ext, ".cpp")
		|| !strcasecmp(ext, ".s")
		|| !strcasecmp(ext, ".asm")) {
		fprint(f, "    <ClCompile Include=\"");
		print_relative_path(f, vcxfile, dir);
		fprintf(f, "%s\" />\r\n", file);
	}
}

static void print_hfile(FILE *f, const char *vcxfile, const char *dir, const char *file) {
	const char *ext = extension(file);
	if (!strcasecmp(ext, ".h")
		|| !strcasecmp(ext, ".hpp")) {
		fprint(f, "    <ClInclude Include=\"");
		print_relative_path(f, vcxfile, dir);
		fprintf(f, "%s\" />\r\n", file);
	}
}

static void print_other(FILE *f, const char *vcxfile, const char *dir, const char *file) {
	const char *ext = extension(file);
	if (!strcasecmp(ext, ".proto")
		|| !strcasecmp(ext, ".txt")
		|| !strcasecmp(ext, ".css")
		|| !strcasecmp(ext, ".js")
		|| !strcasecmp(ext, ".html")
		|| !strcasecmp(ext, ".sh")
		|| !strcasecmp(ext, ".json")) {
		fprint(f, "    <None Include=\"");
		print_relative_path(f, vcxfile, dir);
		fprintf(f, "%s\" />\r\n", file);
	}
}

static void print_ninja(FILE *f, const char *vcxfile, const char *dir, const char *file) {
	const char *ext = extension(file);
	if (!strcasecmp(ext, ".ninja")
		|| !strcasecmp(file, "download.manifest")
		|| !strcasecmp(file, "projects.toml")) {
		fprint(f, "    <None Include=\"");
		print_relative_path(f, vcxfile, dir);
		fprintf(f, "%s\" />\r\n", file);
	}
}

typedef void(*print_fn)(FILE*, const char *vcxfile, const char *dir, const char *file);

static size_t cap_files;
static size_t num_files;
static char **all_files;

static void add_file(const char *file) {
	if (num_files == cap_files) {
		cap_files = (cap_files + 16) * 3 / 2;
		all_files = (char**)realloc(all_files, cap_files * sizeof(char*));
	}
	all_files[num_files++] = strdup(file);
}

#ifdef WIN32
static void list_directory(const char *dir) {
	char *str = malloc(strlen(dir) + 2 + 1);
	strcpy(str, dir);
	strcat(str, "\\*");
	WIN32_FIND_DATA fd;
	HANDLE h = FindFirstFileA(str, &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				add_file(fd.cFileName);
			}
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
}
#else
static void list_directory(const char *dir) {
	DIR *dirp = opendir(dir);
	struct dirent *d;
	if (dirp) {
		while ((d = readdir(dirp)) != NULL) {
			if (d->d_type == DT_REG) {
				add_file(d->d_name);
			}
		}
		closedir(dirp);
	}
}
#endif

static int compare_file(const void *a, const void *b) {
	return strcmp(*(char**)a, *(char**)b);
}

static void replace_char(char *p, char from, char to) {
	while (*p) {
		if (*p == from) {
			*p = to;
		}
		p++;
	}
}

static void print_files(FILE *f, print_fn fn, const char *vcxfile, char *dir) {
	replace_char(dir, '\\', '/');
	list_directory(dir);
	qsort(all_files, num_files, sizeof(char*), &compare_file);
	replace_char(dir, '/', '\\');
	if (num_files) {
		for (size_t i = 0; i < num_files; i++) {
			fn(f, vcxfile, dir, all_files[i]);
			free(all_files[i]);
		}
	} else {
		// maybe we are trying to add just a single file
		char *slash = strrchr(dir, '\\');
		if (slash) {
			*slash = '\0';
			fn(f, vcxfile, dir, slash+1);
			*slash = '\\';
		} else {
			fn(f, vcxfile, "", dir);
		}
	}
	num_files = 0;
}

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

static char *dup_with_replacement(const char *src, const char *test, const char *replacement) {
	size_t sz = 0;
	const char *next, *prev = src;
	while ((next = strstr(prev, test)) != NULL) {
		sz += next - prev;
		sz += strlen(replacement);
		prev = next + strlen(test);
	}
	sz += strlen(prev);

	char *ret = (char*)malloc(sz + 1);
	char *p = ret;
	prev = src;
	while ((next = strstr(prev, test)) != NULL) {
		memcpy(p, prev, next - prev);
		p += next - prev;
		strcpy(p, replacement);
		p += strlen(replacement);
		prev = next + strlen(test);
	}

	strcpy(p, prev);
	return ret;
}

static int is_debug_target(target *t) {
	for (string_list *def = t->defines; def != NULL; def = def->next) {
		if (!strcmp(def->str, "_DEBUG") || !strcmp(def->str, "DEBUG")) {
			return 1;
		}
	}
	return 0;
}

static void write_project(FILE *f, project *p, target *tgts, string_list *sln_defines, string_list *sln_includes, const char *build) {
	fprint(f, "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
		"<Project DefaultTargets=\"Build\" ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n"
		"  <ItemGroup Label=\"ProjectConfigurations\">\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {
		fprintf(f, "    <ProjectConfiguration Include=\"%s|x64\">\r\n", t->vs);
		fprintf(f, "      <Configuration>%s</Configuration>\r\n", t->vs);
		fprint(f, "      <Platform>x64</Platform>\r\n"
			"    </ProjectConfiguration>\r\n");
	}

	fprint(f, "  </ItemGroup>\r\n"
	          "  <ItemGroup>\r\n");
	for (string_list *s = p->dirs; s != NULL; s = s->next) {
		print_files(f, &print_cfile, p->file, s->str);
	}
	fprint(f, "  </ItemGroup>\r\n"
	          "  <ItemGroup>\r\n");
	for (string_list *s = p->dirs; s != NULL; s = s->next) {
		print_files(f, &print_hfile, p->file, s->str);
	}
	fprint(f, "  </ItemGroup>\r\n"
	          "  <ItemGroup>\r\n");
	for (string_list *s = p->dirs; s != NULL; s = s->next) {
		print_files(f, &print_other, p->file, s->str);
	}
	fprint(f, "  </ItemGroup>\r\n"
		"  <PropertyGroup Label=\"Globals\">\r\n");
	fprintf(f, "    <ProjectGuid>%s</ProjectGuid>\r\n", p->uuid);
	fprint(f, "    <Keyword>MakeFileProj</Keyword>\r\n");
	fprintf(f, "    <ProjectName>%s</ProjectName>\r\n", p->name);
	fprintf(f, "  </PropertyGroup>\r\n"
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {
		fprintf(f, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|x64'\" Label=\"Configuration\">\r\n", t->vs);
		fprintf(f, "    <ConfigurationType>Makefile</ConfigurationType>\r\n");
		fprintf(f, "    <UseDebugLibraries>%s</UseDebugLibraries>\r\n", is_debug_target(t) ? "true" : "false");
		fprintf(f, "    <PlatformToolset>v140</PlatformToolset>\r\n"
			"  </PropertyGroup>\r\n");
	}

	fprint(f, "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\r\n"
		"  <ImportGroup Label=\"ExtensionSettings\">\r\n"
		"  </ImportGroup>\r\n"
		"  <ImportGroup Label=\"Shared\">\r\n"
		"  </ImportGroup>\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {
		fprintf(f, "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='%s|x64'\">\r\n", t->vs);
		fprint(f, "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\r\n"
			"  </ImportGroup>\r\n");
	}

	fprintf(f, "  <PropertyGroup Label=\"UserMacros\" />\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {
		char *njtgt = dup_with_replacement(p->target, "{TGT}", t->ninja);
		char *vstgt = strdup(njtgt);
		replace_char(vstgt, '/', '\\');

		fprintf(f, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|x64'\">\r\n", t->vs);
		fprintf(f, "    <NMakeOutput>$(SolutionDir)\\%s</NMakeOutput>\r\n", vstgt);
		fprintf(f, "    <NMakePreprocessorDefinitions>");
		int first_define = 1;
		for (string_list *def = sln_defines; def != NULL; def = def->next) {
			if (!first_define) {
				fputc(';', f);
			}
			fputs(def->str, f);
			first_define = 0;
		}
		for (string_list *def = t->defines; def != NULL; def = def->next) {
			if (!first_define) {
				fputc(';', f);
			}
			fputs(def->str, f);
			first_define = 0;
		}
		fprintf(f, "</NMakePreprocessorDefinitions>\r\n");
		fprintf(f, "    <NMakeBuildCommandLine>%s %s</NMakeBuildCommandLine>\r\n", build, njtgt);
		fprintf(f, "    <NMakeReBuildCommandLine>%s -t clean %s &amp;&amp; %s %s</NMakeReBuildCommandLine>\r\n", build, njtgt, build, njtgt);
		fprintf(f, "    <NMakeCleanCommandLine>%s -t clean %s</NMakeCleanCommandLine>\r\n", build, njtgt);

		fprint(f, "    <NMakeIncludeSearchPath>$(ProjectDir)");
		for (string_list *inc = sln_includes; inc != NULL; inc = inc->next) {
			fprintf(f, ";$(SolutionDir)\\%s", inc->str);
		}
		for (string_list *inc = t->includes; inc != NULL; inc = inc->next) {
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

static void write_command(FILE *f, command *c, target *targets, unsigned add_ninja_files) {
	fprintf(f, "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
		"<Project DefaultTargets=\"Build\" ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n"
		"  <ItemGroup Label=\"ProjectConfigurations\">\r\n");

	for (target *t = targets; t != NULL; t = t->next) {
		fprintf(f, "    <ProjectConfiguration Include=\"%s|x64\">\r\n", t->vs);
		fprintf(f, "      <Configuration>%s</Configuration>\r\n", t->vs);
		fprint(f, "      <Platform>x64</Platform>\r\n"
			"    </ProjectConfiguration>\r\n");
	}
	fprint(f, "  </ItemGroup>\r\n"
	          "  <ItemGroup>\r\n");
	if (add_ninja_files) {
		print_files(f, &print_ninja, "", ".");
	}
	fprint(f, "  </ItemGroup>\r\n");
	fprint(f, "  <PropertyGroup Label=\"Globals\">\r\n");
	fprintf(f, "    <ProjectGuid>%s</ProjectGuid>\r\n", c->uuid);
	fprint(f, "    <Keyword>MakeFileProj</Keyword>\r\n");
	fprintf(f, "    <ProjectName>%s</ProjectName>\r\n", c->name);
	fprint(f, "  </PropertyGroup>\r\n"
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\r\n");

	for (target *t = targets; t != NULL; t = t->next) {
		fprintf(f, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|x64'\" Label=\"Configuration\">\r\n", t->vs);
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
		fprintf(f, "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='%s|x64'\">\r\n", t->vs);
		fprint(f, "    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\r\n"
			"  </ImportGroup>\r\n");
	}

	fprintf(f, "  <PropertyGroup Label=\"UserMacros\" />\r\n");

	for (target *t = targets; t != NULL; t = t->next) {
		char *build = dup_with_replacement(c->build, "{DEFAULT}", t->default_target);
		char *rebuild = dup_with_replacement(c->rebuild, "{DEFAULT}", t->default_target);
		char *clean = dup_with_replacement(c->clean, "{DEFAULT}", t->default_target);

		fprintf(f, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|x64'\">\r\n", t->vs);
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

	fprintf(f, "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"%s\", \"%s\", \"%s\"\r\nEndProject\r\n", "_BUILD_ALL", "_BUILD_ALL.vcxproj", defcmd->uuid);
	if (gencmd) {
		fprintf(f, "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"%s\", \"%s\", \"%s\"\r\nEndProject\r\n", "_GENERATE_VCXPROJ", "_GENERATE_VCXPROJ.vcxproj", gencmd->uuid);
	}

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
			fprintf(f, "\t\t%s.%s|ALL.ActiveCfg = %s|x64\r\n", p->uuid, t->vs, t->vs);
		}
	}

	if (gencmd) {
		for (target *t = targets; t != NULL; t = t->next) {
			fprintf(f, "\t\t%s.%s|ALL.ActiveCfg = %s|x64\r\n", gencmd->uuid, t->vs, t->vs);
		}
	}

	for (target *t = targets; t != NULL; t = t->next) {
		fprintf(f, "\t\t%s.%s|ALL.ActiveCfg = %s|x64\r\n", defcmd->uuid, t->vs, t->vs);
		fprintf(f, "\t\t%s.%s|ALL.Build.0 = %s|x64\r\n", defcmd->uuid, t->vs, t->vs);
	}

	fprint(f, "\tEndGlobalSection\r\n"
		"\tGlobalSection(SolutionProperties) = preSolution\r\n"
		"\t\tHideSolutionNode = FALSE\r\n"
		"\tEndGlobalSection\r\n"
		"EndGlobal\r\n");
}

int main(int argc, char *argv[]) {
	if (argc > 1 && chdir(argv[1])) {
		fprintf(stderr, "failed to change directory to %s\n", argv[1]);
		return 1;
	}

	FILE *f = must_fopen("projects.toml", "r");
	char errbuf[128];
	toml_table_t *root = toml_parse_file(f, errbuf, sizeof(errbuf));
	if (root == NULL) {
		fprintf(stderr, "parse error with config file projects.toml: %s\n", errbuf);
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
		t->defines = get_string_list(tbl, "defines");
		t->includes = get_string_list(tbl, "includes");
		for (string_list *inc = t->includes; inc != NULL; inc = inc->next) {
			replace_char(inc->str, '/', '\\');
		}
		*ptarg = t;
		ptarg = &t->next;
	}

	string_list *defines = get_string_list(root, "defines");
	string_list *includes = get_string_list(root, "includes");
	const char *slnfile = must_get_string(root, "solution");
	const char *build = must_get_string(root, "build");
	const char *generate = get_string(root, "generate", NULL);

	for (string_list *inc = includes; inc != NULL; inc = inc->next) {
		replace_char(inc->str, '/', '\\');
	}


	char buildall[1024], rebuildall[2048], cleanall[1024];
	snprintf(buildall, sizeof(buildall), "%s {DEFAULT}", build);
	snprintf(rebuildall, sizeof(rebuildall), "%s -t clean {DEFAULT} &amp;&amp; %s {DEFAULT}", build, build);
	snprintf(cleanall, sizeof(cleanall), "%s -t clean {DEFAULT}", build);

	command def;
	def.name = "_BUILD_ALL";
	def.build = buildall;
	def.rebuild = rebuildall;
	def.clean = cleanall;
	generate_uuid(def.uuid, "_BUILD_ALL.vcxproj");

	printf("generating _BUILD_ALL.vcxproj\n");
	f = must_fopen("_BUILD_ALL.vcxproj", "wb");
	write_command(f, &def, targets, 1);
	fclose(f);

	command gen;
	if (generate) {
		gen.name = "_GENERATE_VCXPROJ";
		gen.build = generate;
		gen.rebuild = generate;
		gen.clean = "";
		generate_uuid(gen.uuid, "_GENERATE_VCXPROJ.vcxproj");

		printf("generating _GENERATE_VCXPROJ.vcxproj\n");
		f = must_fopen("_GENERATE_VCXPROJ.vcxproj", "wb");
		write_command(f, &gen, targets, 0);
		fclose(f);
	}

	for (project *p = projects; p != NULL; p = p->next) {
		printf("generating %s\n", p->file);
		f = must_fopen(p->file, "wb");
		generate_uuid(p->uuid, p->file);
		replace_char(p->file, '/', '\\');
		write_project(f, p, targets, defines, includes, build);
		fclose(f);
	}

	printf("generating %s\n", slnfile);
	f = must_fopen(slnfile, "wb");
	write_solution(f, projects, targets, &def, generate ? &gen : NULL);
	fclose(f);

	return 0;
}
