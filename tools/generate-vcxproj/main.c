#include <stdio.h>
#include <stdint.h>
#include "tomlc99/toml.h"
#include "bearssl_wrapper.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#endif

#ifdef _MSC_VER
#define strcasecmp(a,b) _stricmp(a,b)
#endif

typedef struct project project;
typedef struct target target;
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
	char uuid[39];
	string_list *dirs;
};

struct target {
	struct target *next;
	char *vs;
	char *ninja;
	char *default_target;
	char *defines;
};

static const char *get_string(toml_table_t *t, const char *key, const char *defstr) {
	const char *val = toml_raw_in(t, key);
	if (toml_rtos(val, &val)) {
		return defstr;
	}
	return val;
}

static const char *must_get_string(toml_table_t *t, const char *key) {
	const char *val = get_string(t, key, NULL);
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
	char *val;
	while ((val = toml_raw_at(array, idx++)) != NULL) {
		if (toml_rtos(val, &val)) {
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
			fn(f, dir, fd.cFileName);
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
}
#else
#endif

static void generate_uuid(project *p) {
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
	br_sha1_update(&ctx, p->file, strlen(p->file));

	unsigned char h[br_sha1_SIZE];
	br_sha1_out(&ctx, h);
	h[6] = (h[6] & 0xF) | 0x50; // set version
	h[8] = (h[8] & 0x3F) | 0x80; // set variant
	snprintf(p->uuid, sizeof(p->uuid), "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		h[0], h[1], h[2], h[3],
		h[4], h[5],
		h[6], h[7],
		h[8], h[9],
		h[10], h[11], h[12], h[13], h[14], h[15]);
}

static void write_project(FILE *f, project *p, target *tgts) {
	fprintf(f, "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
	fprintf(f, "<Project DefaultTargets=\"Build\" ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n");
	fprintf(f, "  <ItemGroup Label=\"ProjectConfigurations\">\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {
		fprintf(f, "    <ProjectConfiguration Include=\"%s|Win32\">\r\n", t->vs);
		fprintf(f, "      <Configuration>%s</Configuration>\r\n", t->vs);
		fprintf(f, "      <Platform>Win32</Platform>\r\n");
		fprintf(f, "    </ProjectConfiguration>\r\n");
	}

	fprintf(f, "  </ItemGroup>\r\n");

	fprintf(f, "  <ItemGroup>\r\n");
	for (string_list *s = p->dirs; s != NULL; s = s->next) {
		list_directory(f, &print_cfile, s->str);
	}
	fprintf(f, "  </ItemGroup>\r\n");

	fprintf(f, "  <ItemGroup>\r\n");
	for (string_list *s = p->dirs; s != NULL; s = s->next) {
		list_directory(f, &print_hfile, s->str);
	}
	fprintf(f, "  </ItemGroup>\r\n");


	fprintf(f, "  <ItemGroup>\r\n");
	for (string_list *s = p->dirs; s != NULL; s = s->next) {
		list_directory(f, &print_other, s->str);
	}
	fprintf(f, "  </ItemGroup>\r\n");

	fprintf(f, "  <PropertyGroup Label=\"Globals\">\r\n");
	fprintf(f, "    <ProjectGuid>%s</ProjectGuid>\r\n", p->uuid);
	fprintf(f, "    <Keyword>MakeFileProj</Keyword>\r\n");
	fprintf(f, "    <ProjectName>%s</ProjectName>\r\n", p->name);
	fprintf(f, "  </PropertyGroup>\r\n");
	fprintf(f, "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\r\n");

	for (target *t = tgts; t != NULL; t = t->next) {

	}
}

int main(int argc, char *argv[]) {
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
		generate_uuid(p);
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

	return 0;
}
