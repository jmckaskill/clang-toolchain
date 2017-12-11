package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

type project struct {
	Name   string
	File   string
	Empty  bool
	Dirs   []string // defaults to the folder the file is in unless empty is set
	UUID   string
	Target string
}

type target struct {
	VS      string
	Ninja   string
	Defines string
}

type config struct {
	Solution     string
	Includes     []string
	Projects     []*project
	DefaultUUID  string
	GenerateUUID string
	Targets      []*target
}

func replace(name string, data []byte) {
	have, _ := ioutil.ReadFile(name)
	if !bytes.Equal(have, data) {
		fmt.Fprintf(os.Stdout, "UPDATING")
		os.MkdirAll(filepath.Dir(name), os.ModePerm)
		f, err := os.Create(name)
		must(err)
		defer f.Close()
		f.Write(data)
	}
	fmt.Fprintf(os.Stdout, "\n")
}

func writeSolution(c *config) {
	buf := bytes.Buffer{}
	fmt.Fprintf(os.Stdout, "generating %s ... ", c.Solution)

	buf.Write([]byte("\xEF\xBB\xBF\r\n" +
		"Microsoft Visual Studio Solution File, Format Version 12.00\r\n" +
		"# Visual Studio 14\r\n" +
		"VisualStudioVersion = 14.0.25420.1\r\n" +
		"MinimumVisualStudioVersion = 10.0.40219.1\r\n"))

	if c.DefaultUUID != "" {
		fmt.Fprintf(&buf, "Project(\"%s\") = \"%s\", \"%s\", \"%s\"\r\nEndProject\r\n",
			"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",
			"_DEFAULT",
			"_DEFAULT.vcxproj",
			c.DefaultUUID)
	}

	if c.GenerateUUID != "" {
		fmt.Fprintf(&buf, "Project(\"%s\") = \"%s\", \"%s\", \"%s\"\r\nEndProject\r\n",
			"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",
			"_GENERATE PROJECTS",
			"_GENERATE PROJECTS.vcxproj",
			c.GenerateUUID)
	}

	for _, prj := range c.Projects {
		fmt.Fprintf(&buf, "Project(\"%s\") = \"%s\", \"%s\", \"%s\"\r\nEndProject\r\n",
			"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",
			prj.Name,
			strings.Replace(prj.File, "/", "\\", -1),
			prj.UUID)
	}

	fmt.Fprintf(&buf, "Global\r\n\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\r\n")
	for _, tgt := range c.Targets {
		fmt.Fprintf(&buf, "\t\t%s|ALL = %s|ALL\r\n", tgt.VS, tgt.VS)
	}
	fmt.Fprintf(&buf, "\tEndGlobalSection\r\n")

	fmt.Fprintf(&buf, "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\r\n")
	for _, prj := range c.Projects {
		for _, tgt := range c.Targets {
			fmt.Fprintf(&buf, "\t\t%s.%s|ALL.ActiveCfg = %s|Win32\r\n", prj.UUID, tgt.VS, tgt.VS)
		}
	}

	if c.GenerateUUID != "" {
		for _, tgt := range c.Targets {
			fmt.Fprintf(&buf, "\t\t%s.%s|ALL.ActiveCfg = %s|Win32\r\n", c.GenerateUUID, tgt.VS, tgt.VS)
		}
	}

	if c.DefaultUUID != "" {
		for _, tgt := range c.Targets {
			fmt.Fprintf(&buf, "\t\t%s.%s|ALL.ActiveCfg = %s|Win32\r\n", c.DefaultUUID, tgt.VS, tgt.VS)
			fmt.Fprintf(&buf, "\t\t%s.%s|ALL.Build.0 = %s|Win32\r\n", c.DefaultUUID, tgt.VS, tgt.VS)
		}
	}

	fmt.Fprintf(&buf, "\tEndGlobalSection\r\n")

	buf.Write([]byte("\tGlobalSection(SolutionProperties) = preSolution\r\n" +
		"\t\tHideSolutionNode = FALSE\r\n" +
		"\tEndGlobalSection\r\n" +
		"EndGlobal\r\n"))

	replace(c.Solution, buf.Bytes())
}

func writeProject(c *config, prj *project, toolchain string) {
	fmt.Fprintf(os.Stdout, "generating %s ... ", prj.File)

	buf := bytes.Buffer{}
	dirs := prj.Dirs
	if len(dirs) == 0 && !prj.Empty {
		dirs = []string{filepath.Dir(prj.File)}
	}

	cfiles := []string{}
	hfiles := []string{}
	ofiles := []string{}

	for _, d := range dirs {
		filepath.Walk(d, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			} else if info.IsDir() && path == d {
				return nil
			} else if info.IsDir() {
				return filepath.SkipDir
			}

			relfn, err := filepath.Rel(filepath.Dir(prj.File), path)
			if err != nil {
				return err
			}
			relfn = strings.Replace(relfn, "/", "\\", -1)

			switch filepath.Ext(path) {
			case ".c", ".cpp", ".S", ".s", ".asm":
				cfiles = append(cfiles, relfn)
			case ".h", ".H", ".hpp":
				hfiles = append(hfiles, relfn)
			case ".proto", ".txt", ".css", ".js", ".html", ".sh", ".json":
				ofiles = append(ofiles, relfn)
			}

			return nil
		})
	}

	sort.Strings(cfiles)
	sort.Strings(hfiles)
	sort.Strings(ofiles)

	buf.Write([]byte("\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n" +
		"<Project DefaultTargets=\"Build\" ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n" +
		"  <ItemGroup Label=\"ProjectConfigurations\">\r\n"))

	for _, tgt := range c.Targets {
		fmt.Fprintf(&buf, "    <ProjectConfiguration Include=\"%s|Win32\">\r\n", tgt.VS)
		fmt.Fprintf(&buf, "      <Configuration>%s</Configuration>\r\n", tgt.VS)
		fmt.Fprintf(&buf, "      <Platform>Win32</Platform>\r\n")
		fmt.Fprintf(&buf, "    </ProjectConfiguration>\r\n")
	}

	fmt.Fprintf(&buf, "  </ItemGroup>\r\n")

	fmt.Fprintf(&buf, "  <ItemGroup>\r\n")
	for _, fn := range cfiles {
		fmt.Fprintf(&buf, "    <ClCompile Include=\"%s\" />\r\n", fn)
	}
	fmt.Fprintf(&buf, "  </ItemGroup>\r\n")

	fmt.Fprintf(&buf, "  <ItemGroup>\r\n")
	for _, fn := range hfiles {
		fmt.Fprintf(&buf, "    <ClInclude Include=\"%s\" />\r\n", fn)
	}
	fmt.Fprintf(&buf, "  </ItemGroup>\r\n")

	fmt.Fprintf(&buf, "  <ItemGroup>\r\n")
	for _, fn := range ofiles {
		fmt.Fprintf(&buf, "    <None Include=\"%s\" />\r\n", fn)
	}
	fmt.Fprintf(&buf, "  </ItemGroup>\r\n")

	fmt.Fprintf(&buf, "  <PropertyGroup Label=\"Globals\">\r\n"+
		"    <ProjectGuid>%s</ProjectGuid>\r\n"+
		"    <Keyword>MakeFileProj</Keyword>\r\n"+
		"    <ProjectName>%s</ProjectName>\r\n"+
		"  </PropertyGroup>\r\n"+
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\r\n",
		prj.UUID, prj.Name)

	for _, tgt := range c.Targets {
		debug := "false"
		if tgt.Ninja == "win32-debug" {
			debug = "true"
		}
		fmt.Fprintf(&buf, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\" Label=\"Configuration\">\r\n"+
			"    <ConfigurationType>Makefile</ConfigurationType>\r\n"+
			"    <UseDebugLibraries>%s</UseDebugLibraries>\r\n"+
			"    <PlatformToolset>v140</PlatformToolset>\r\n"+
			"  </PropertyGroup>\r\n",
			tgt.VS, debug)
	}

	fmt.Fprintf(&buf, "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\r\n"+
		"  <ImportGroup Label=\"ExtensionSettings\">\r\n"+
		"  </ImportGroup>\r\n"+
		"  <ImportGroup Label=\"Shared\">\r\n"+
		"  </ImportGroup>\r\n")

	for _, tgt := range c.Targets {
		fmt.Fprintf(&buf, "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\">\r\n"+
			"    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\r\n"+
			"  </ImportGroup>\r\n",
			tgt.VS)
	}

	fmt.Fprintf(&buf, "  <PropertyGroup Label=\"UserMacros\" />\r\n")

	for _, tgt := range c.Targets {
		njtgt := strings.Replace(prj.Target, "{TGT}", tgt.Ninja, 1)
		vstgt := strings.Replace(njtgt, "/", "\\", -1)

		fmt.Fprintf(&buf, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\">\r\n", tgt.VS)
		fmt.Fprintf(&buf, "    <NMakeOutput>$(SolutionDir)\\%s</NMakeOutput>\r\n", vstgt)
		fmt.Fprintf(&buf, "    <NMakePreprocessorDefinitions>%s</NMakePreprocessorDefinitions>\r\n", tgt.Defines)
		fmt.Fprintf(&buf, "    <NMakeBuildCommandLine>%s\\install.exe &amp;&amp; %s\\host\\bin\\ninja.exe -C $(SolutionDir) -f msvc.ninja %s</NMakeBuildCommandLine>\r\n",
			toolchain, toolchain, njtgt)
		fmt.Fprintf(&buf, "    <NMakeReBuildCommandLine>%s\\install.exe &amp;&amp; %s\\host\\bin\\ninja.exe -C $(SolutionDir) -f msvc.ninja -t clean %s &amp;&amp; %s\\host\\bin\\ninja.exe -C $(SolutionDir) -f msvc.ninja %s</NMakeReBuildCommandLine>\r\n",
			toolchain, toolchain, njtgt, toolchain, njtgt)
		fmt.Fprintf(&buf, "    <NMakeCleanCommandLine>%s\\install.exe &amp;&amp; %s\\host\\bin\\ninja.exe -C $(SolutionDir) -f msvc.ninja -t clean %s</NMakeCleanCommandLine>\r\n",
			toolchain, toolchain, njtgt)

		fmt.Fprintf(&buf, "    <NMakeIncludeSearchPath>$(ProjectDir)")
		for _, inc := range c.Includes {
			fmt.Fprintf(&buf, ";$(SolutionDir)\\%s", strings.Replace(inc, "/", "\\", -1))
		}
		fmt.Fprintf(&buf, "</NMakeIncludeSearchPath>\r\n")
		fmt.Fprintf(&buf, "    <IntDir>$(SolutionDir)\\obj\\$(Configuration)\\</IntDir>\r\n")
		fmt.Fprintf(&buf, "    <SourcePath />\r\n")
		fmt.Fprintf(&buf, "    <ExcludePath />\r\n")
		fmt.Fprintf(&buf, "  </PropertyGroup>\r\n")
	}

	buf.Write([]byte("  <ItemDefinitionGroup>\r\n" +
		"  </ItemDefinitionGroup>\r\n" +
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\r\n" +
		"  <ImportGroup Label=\"ExtensionTargets\">\r\n" +
		"  </ImportGroup>\r\n" +
		"</Project>\r\n"))

	replace(prj.File, buf.Bytes())
}

func writeCommand(c *config, name, file, uuid, build, rebuild, clean string) {
	fmt.Fprintf(os.Stdout, "generating %s ... ", file)

	buf := bytes.Buffer{}
	buf.Write([]byte("\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n" +
		"<Project DefaultTargets=\"Build\" ToolsVersion=\"14.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n" +
		"  <ItemGroup Label=\"ProjectConfigurations\">\r\n"))

	for _, tgt := range c.Targets {
		fmt.Fprintf(&buf, "    <ProjectConfiguration Include=\"%s|Win32\">\r\n", tgt.VS)
		fmt.Fprintf(&buf, "      <Configuration>%s</Configuration>\r\n", tgt.VS)
		fmt.Fprintf(&buf, "      <Platform>Win32</Platform>\r\n")
		fmt.Fprintf(&buf, "    </ProjectConfiguration>\r\n")
	}

	fmt.Fprintf(&buf, "  </ItemGroup>\r\n")

	fmt.Fprintf(&buf, "  <PropertyGroup Label=\"Globals\">\r\n"+
		"    <ProjectGuid>%s</ProjectGuid>\r\n"+
		"    <Keyword>MakeFileProj</Keyword>\r\n"+
		"    <ProjectName>%s</ProjectName>\r\n"+
		"  </PropertyGroup>\r\n"+
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\r\n",
		uuid, name)

	for _, tgt := range c.Targets {
		fmt.Fprintf(&buf, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\" Label=\"Configuration\">\r\n"+
			"    <ConfigurationType>Makefile</ConfigurationType>\r\n"+
			"    <UseDebugLibraries>false</UseDebugLibraries>\r\n"+
			"    <PlatformToolset>v140</PlatformToolset>\r\n"+
			"  </PropertyGroup>\r\n",
			tgt.VS)
	}

	fmt.Fprintf(&buf, "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\r\n"+
		"  <ImportGroup Label=\"ExtensionSettings\">\r\n"+
		"  </ImportGroup>\r\n"+
		"  <ImportGroup Label=\"Shared\">\r\n"+
		"  </ImportGroup>\r\n")

	for _, tgt := range c.Targets {
		fmt.Fprintf(&buf, "  <ImportGroup Label=\"PropertySheets\" Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\">\r\n"+
			"    <Import Project=\"$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\" Condition=\"exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')\" Label=\"LocalAppDataPlatform\" />\r\n"+
			"  </ImportGroup>\r\n",
			tgt.VS)
	}

	fmt.Fprintf(&buf, "  <PropertyGroup Label=\"UserMacros\" />\r\n")

	for _, tgt := range c.Targets {
		njbuild := strings.Replace(build, "{TGT}", tgt.Ninja, -1)
		njrebuild := strings.Replace(rebuild, "{TGT}", tgt.Ninja, -1)
		njclean := strings.Replace(clean, "{TGT}", tgt.Ninja, -1)

		fmt.Fprintf(&buf, "  <PropertyGroup Condition=\"'$(Configuration)|$(Platform)'=='%s|Win32'\">\r\n", tgt.VS)
		fmt.Fprintf(&buf, "    <NMakeBuildCommandLine>%s</NMakeBuildCommandLine>\r\n", njbuild)
		fmt.Fprintf(&buf, "    <NMakeReBuildCommandLine>%s</NMakeReBuildCommandLine>\r\n", njrebuild)
		fmt.Fprintf(&buf, "    <NMakeCleanCommandLine>%s</NMakeCleanCommandLine>\r\n", njclean)
		fmt.Fprintf(&buf, "  </PropertyGroup>\r\n")
	}

	buf.Write([]byte("  <ItemDefinitionGroup>\r\n" +
		"  </ItemDefinitionGroup>\r\n" +
		"  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\r\n" +
		"  <ImportGroup Label=\"ExtensionTargets\">\r\n" +
		"  </ImportGroup>\r\n" +
		"</Project>\r\n"))

	replace(file, buf.Bytes())
}

func must(err error) {
	if err != nil {
		log.Fatal(err)
	}
}

func main() {

	if len(os.Args) != 2 {
		log.Fatal("usage generate-vcxproj.exe config.json")
	}

	f, err := os.Open(os.Args[1])
	must(err)
	defer f.Close()

	cfg := config{}
	must(json.NewDecoder(f).Decode(&cfg))

	dir, err := filepath.Abs(filepath.Dir(os.Args[1]))
	must(err)
	must(os.Chdir(dir))

	exefn, err := os.Executable()
	must(err)
	tooldir, err := filepath.Rel(dir, filepath.Dir(exefn))
	must(err)
	toolchain := "$(SolutionDir)\\" + strings.Replace(tooldir, "/", "\\", -1)

	cfg.DefaultUUID = strings.ToUpper(cfg.DefaultUUID)
	cfg.GenerateUUID = strings.ToUpper(cfg.GenerateUUID)

	for _, prj := range cfg.Projects {
		prj.UUID = strings.ToUpper(prj.UUID)
	}

	writeSolution(&cfg)
	for _, prj := range cfg.Projects {
		writeProject(&cfg, prj, toolchain)
	}

	if cfg.GenerateUUID != "" {
		cmd := fmt.Sprintf("%s\\generate-vcxproj.exe %s", toolchain, os.Args[1])
		writeCommand(&cfg, "_GENERATE PROJECTS", "_GENERATE PROJECTS.vcxproj", cfg.GenerateUUID, cmd, cmd, "")
	}

	if cfg.DefaultUUID != "" {
		build := fmt.Sprintf("%s\\install.exe &amp;&amp; %s\\host\\bin\\ninja.exe -f msvc.ninja {TGT}", toolchain, toolchain)
		rebuild := fmt.Sprintf("%s\\install.exe &amp;&amp; %s\\host\\bin\\ninja.exe -f msvc.ninja -t clean {TGT} &amp;&amp; %s\\host\\bin\\ninja.exe -f msvc.ninja {TGT}", toolchain, toolchain, toolchain)
		clean := fmt.Sprintf("%s\\install.exe &amp;&amp; %s\\host\\bin\\ninja.exe -f msvc.ninja -t clean {TGT}", toolchain, toolchain)
		writeCommand(&cfg, "_DEFAULT", "_DEFAULT.vcxproj", cfg.DefaultUUID, build, rebuild, clean)
	}
}
