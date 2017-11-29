PROJECT_LIST=""
PROJECT_SECTIONS=""

write_solution() {
  FILE="$1"

  echo "GENERATING $FILE"

  
  cat > $FILE <<EOF
Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio 14
VisualStudioVersion = 14.0.25123.0
MinimumVisualStudioVersion = 10.0.40219.1${PROJECT_LIST}
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		ARM-Debug|ALL = ARM-Debug|ALL
		ARM-Release|ALL = ARM-Release|ALL
		Debug|ALL = Debug|ALL
		Release|ALL = Release|ALL
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution${PROJECT_SECTIONS}
	EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
EndGlobal
EOF

  unix2dos --add-bom $FILE &> /dev/null

  PROJECT_LIST=""
  PROJECT_SECTIONS=""
}


do_write_project() {
  DIR="$1"
  NAME="$2"
  TYPE="$3"
  BUILD_TARGET="$4"
  DEBUG_TARGET="$5"
  UUID=`echo $6 | tr [a-z] [A-Z]`
  shift; shift; shift; shift; shift; shift
  FOLDERS="$@"

  PROJECT_LIST="$PROJECT_LIST
Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"$TYPE - $BUILD_TARGET\", \"$DIR\\$NAME.vcxproj\", \"$UUID\"
EndProject"

  PROJECT_SECTIONS="$PROJECT_SECTIONS
		$UUID.ARM-Debug|ALL.ActiveCfg = ARM-Debug|Win32
		$UUID.ARM-Release|ALL.ActiveCfg = ARM-Release|Win32
		$UUID.Debug|ALL.ActiveCfg = Debug|Win32
		$UUID.Release|ALL.ActiveCfg = Release|Win32"

  echo "GENERATING $DIR/$NAME.vcxproj"

  pushd $DIR &> /dev/null
  FILE="$NAME.vcxproj"

  cat > $FILE <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
	<ProjectConfiguration Include="Release|Win32">
	  <Configuration>Release</Configuration>
	  <Platform>Win32</Platform>
	</ProjectConfiguration>
	<ProjectConfiguration Include="Debug|Win32">
	  <Configuration>Debug</Configuration>
	  <Platform>Win32</Platform>
	</ProjectConfiguration>
	<ProjectConfiguration Include="ARM-Debug|Win32">
	  <Configuration>ARM-Debug</Configuration>
	  <Platform>Win32</Platform>
	</ProjectConfiguration>
	<ProjectConfiguration Include="ARM-Release|Win32">
	  <Configuration>ARM-Release</Configuration>
	  <Platform>Win32</Platform>
	</ProjectConfiguration>
  </ItemGroup>
EOF


  if [ -n "$FOLDERS" ]; then
	echo '  <ItemGroup>' >> $FILE
	for file in `find $FOLDERS -iname '*.c' -o -iname '*.cpp' -o -iname '*.S'`; do
	  echo "    <ClCompile Include=\"$file\" />" >> $FILE
	done
	echo '  </ItemGroup>' >> $FILE

	echo '  <ItemGroup>' >> $FILE
	for file in `find $FOLDERS -iname '*.h' -o -iname '*.hpp'`; do
	  echo "    <ClInclude Include=\"$file\" />" >> $FILE
	done
	echo '  </ItemGroup>' >> $FILE

	echo '  <ItemGroup>' >> $FILE
	for file in `find $FOLDERS -iname '*.ninja' -o -iname '*.proto' -o -iname '*.txt' -o -iname '*.css' -o -iname '*.html' -o -iname '*.js' -o -iname '*.json' -o -iname '*.sh'`; do
	  echo "    <None Include=\"$file\" />" >> $FILE
	done
	echo '  </ItemGroup>' >> $FILE
  fi

  cat >> $FILE <<EOF
  <PropertyGroup Label="Globals">
	<ProjectGuid>${UUID}</ProjectGuid>
	<Keyword>MakeFileProj</Keyword>
	<ProjectName>${TYPE} - ${BUILD_TARGET}</ProjectName>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>false</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>true</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Release|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>false</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Debug|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>false</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings">
  </ImportGroup>
  <ImportGroup Label="Shared">
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
	<Import Project="\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
	<Import Project="\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='ARM-Release|Win32'">
	<Import Project="\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='ARM-Debug|Win32'">
	<Import Project="\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
	<NMakeOutput>\$(SolutionDir)\\bin\\win32-release\\${DEBUG_TARGET}</NMakeOutput>
	<NMakePreprocessorDefinitions>WIN32;NDEBUG</NMakePreprocessorDefinitions>
	<NMakeBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/win32-release/${BUILD_TARGET}</NMakeBuildCommandLine>
	<NMakeReBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/win32-release/${BUILD_TARGET} &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/win32-release/${BUILD_TARGET}</NMakeReBuildCommandLine>
	<NMakeCleanCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/win32-release/${BUILD_TARGET}</NMakeCleanCommandLine>
	<NMakeIncludeSearchPath>\$(SolutionDir);\$(ProjectDir);${INCLUDEDIRS}</NMakeIncludeSearchPath>
	<IntDir>\$(SolutionDir)\\obj\\\$(Configuration)\</IntDir>
	<SourcePath />
	<ExcludePath />
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
	<NMakeOutput>\$(SolutionDir)\\bin\\win32-debug\\${DEBUG_TARGET}</NMakeOutput>
	<NMakePreprocessorDefinitions>WIN32</NMakePreprocessorDefinitions>
	<NMakeBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/win32-debug/${BUILD_TARGET}</NMakeBuildCommandLine>
	<NMakeReBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/win32-debug/${BUILD_TARGET} &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/win32-debug/${BUILD_TARGET}</NMakeReBuildCommandLine>
	<NMakeCleanCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/win32-debug/${BUILD_TARGET}</NMakeCleanCommandLine>
	<NMakeIncludeSearchPath>\$(SolutionDir);\$(ProjectDir);${INCLUDEDIRS}</NMakeIncludeSearchPath>
	<IntDir>\$(SolutionDir)\\obj\\\$(Configuration)\</IntDir>
	<SourcePath />
	<ExcludePath />
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Release|Win32'">
	<NMakeOutput>\$(SolutionDir)\\bin\\arm-release\\${DEBUG_TARGET}</NMakeOutput>
	<NMakePreprocessorDefinitions>__linux__;__arm__;NDEBUG</NMakePreprocessorDefinitions>
	<NMakeBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/arm-release/${BUILD_TARGET}</NMakeBuildCommandLine>
	<NMakeReBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/arm-release/${BUILD_TARGET} &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/arm-release/${BUILD_TARGET}</NMakeReBuildCommandLine>
	<NMakeCleanCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/arm-release/${BUILD_TARGET}</NMakeCleanCommandLine>
	<NMakeIncludeSearchPath>\$(SolutionDir);\$(ProjectDir);${INCLUDEDIRS};\$(SolutionDir)\\toolchain\\lib\\arm\\include;\$(SolutionDir)\\toolchain\\host\\lib\\clang\\4.0.1\\include</NMakeIncludeSearchPath>
	<IntDir>\$(SolutionDir)\\obj\\\$(Configuration)\</IntDir>
	<SourcePath />
	<ExcludePath />
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Debug|Win32'">
	<NMakeOutput>\$(SolutionDir)\\bin\\arm-debug\\${DEBUG_TARGET}</NMakeOutput>
	<NMakePreprocessorDefinitions>__linux__;__arm__;DEBUG</NMakePreprocessorDefinitions>
	<NMakeBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/arm-debug/${BUILD_TARGET}</NMakeBuildCommandLine>
	<NMakeReBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/arm-debug/${BUILD_TARGET} &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/arm-debug/${BUILD_TARGET}</NMakeReBuildCommandLine>
	<NMakeCleanCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/arm-debug/${BUILD_TARGET}</NMakeCleanCommandLine>
	<NMakeIncludeSearchPath>\$(SolutionDir);\$(ProjectDir);${INCLUDEDIRS};\$(SolutionDir)\\toolchain\\lib\\arm\\include;\$(SolutionDir)\\toolchain\\host\\lib\\clang\\4.0.1\\include</NMakeIncludeSearchPath>
	<IntDir>\$(SolutionDir)\\obj\\\$(Configuration)\</IntDir>
	<SourcePath />
	<ExcludePath />
  </PropertyGroup>
  <ItemDefinitionGroup>
  </ItemDefinitionGroup>
  <Import Project="\$(VCTargetsPath)\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
  </ImportGroup>
</Project>
EOF

  unix2dos --add-bom $FILE &> /dev/null
  popd &> /dev/null
}

write_sg6() {
  DIR="$1"
  NAME="$2"
  DEBUG_TARGET="$3"
  UUID=`echo $4 | tr [a-z] [A-Z]`
  shift; shift; shift; shift
  FOLDERS="$*"

  do_write_project "$DIR" "$NAME" sg6 "$NAME.sg6" "$DEBUG_TARGET" "$UUID" "$FOLDERS"

  PROJECT_SECTIONS="$PROJECT_SECTIONS
		$UUID.ARM-Debug|ALL.Build.0 = ARM-Debug|Win32
		$UUID.ARM-Release|ALL.Build.0 = ARM-Release|Win32
		$UUID.Debug|ALL.Build.0 = Debug|Win32
		$UUID.Release|ALL.Build.0 = Release|Win32"
}

write_exe() {
  DIR="$1"
  NAME="$2"
  UUID=`echo $3 | tr [a-z] [A-Z]`
  shift; shift; shift
  FOLDERS="$*"

  do_write_project "$DIR" "$NAME" bin "$NAME.exe" "$NAME.exe" "$UUID" "$FOLDERS"
}

write_lib() {
  DIR="$1"
  NAME="$2"
  UUID=`echo $3 | tr [a-z] [A-Z]`
  shift; shift; shift
  FOLDERS="$*"

  do_write_project "$DIR" "$NAME" lib "$NAME.lib" "$NAME.lib" "$UUID" "$FOLDERS"
}

write_command() {
  NAME="$1"
  UUID=`echo $2 | tr [a-z] [A-Z]`
  shift; shift
  COMMAND="$*"

  FILE="$NAME.vcxproj"

  echo "GENERATING $FILE"

  PROJECT_LIST="$PROJECT_LIST
Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"$NAME\", \"$FILE\", \"$UUID\"
EndProject"

  PROJECT_SECTIONS="$PROJECT_SECTIONS
		$UUID.ARM-Debug|ALL.ActiveCfg = ARM-Debug|Win32
		$UUID.ARM-Release|ALL.ActiveCfg = ARM-Release|Win32
		$UUID.Debug|ALL.ActiveCfg = Debug|Win32
		$UUID.Release|ALL.ActiveCfg = Release|Win32"

  cat > "$FILE" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
	<ProjectConfiguration Include="Release|Win32">
	  <Configuration>Release</Configuration>
	  <Platform>Win32</Platform>
	</ProjectConfiguration>
  <ProjectConfiguration Include="Debug|Win32">
	<Configuration>Debug</Configuration>
	<Platform>Win32</Platform>
  </ProjectConfiguration>
  <ProjectConfiguration Include="ARM-Debug|Win32">
	<Configuration>ARM-Debug</Configuration>
	<Platform>Win32</Platform>
  </ProjectConfiguration>
  <ProjectConfiguration Include="ARM-Release|Win32">
	<Configuration>ARM-Release</Configuration>
	<Platform>Win32</Platform>
  </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
	<ProjectGuid>${UUID}</ProjectGuid>
	<Keyword>MakeFileProj</Keyword>
	<ProjectName>${NAME}</ProjectName>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>false</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>true</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Release|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>false</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Debug|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>false</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings">
  </ImportGroup>
  <ImportGroup Label="Shared">
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
	<Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
	<Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='ARM-Debug|Win32'">
	<Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='ARM-Release|Win32'">
	<Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
	<NMakeBuildCommandLine>${COMMAND}</NMakeBuildCommandLine>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
	<NMakeBuildCommandLine>${COMMAND}</NMakeBuildCommandLine>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Debug|Win32'">
	<NMakeBuildCommandLine>${COMMAND}</NMakeBuildCommandLine>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Release|Win32'">
	<NMakeBuildCommandLine>${COMMAND}</NMakeBuildCommandLine>
  </PropertyGroup>
  <ItemDefinitionGroup>
  </ItemDefinitionGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
  </ImportGroup>
</Project>
EOF
  unix2dos --add-bom "$FILE" &> /dev/null
}

write_default() {
  NAME="$1"
  UUID=`echo $2 | tr [a-z] [A-Z]`
  shift; shift
  COMMAND="$*"

  FILE="$NAME.vcxproj"

  echo "GENERATING $FILE"

  PROJECT_LIST="$PROJECT_LIST
Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"$NAME\", \"$FILE\", \"$UUID\"
EndProject"

  PROJECT_SECTIONS="$PROJECT_SECTIONS
		$UUID.ARM-Debug|ALL.ActiveCfg = ARM-Debug|Win32
		$UUID.ARM-Debug|ALL.Build.0 = ARM-Debug|Win32
		$UUID.ARM-Release|ALL.ActiveCfg = ARM-Release|Win32
		$UUID.ARM-Release|ALL.Build.0 = ARM-Release|Win32
		$UUID.Debug|ALL.ActiveCfg = Debug|Win32
		$UUID.Debug|ALL.Build.0 = Debug|Win32
		$UUID.Release|ALL.ActiveCfg = Release|Win32
		$UUID.Release|ALL.Build.0 = Release|Win32"

  cat > "$FILE" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
	<ProjectConfiguration Include="Release|Win32">
	  <Configuration>Release</Configuration>
	  <Platform>Win32</Platform>
	</ProjectConfiguration>
  <ProjectConfiguration Include="Debug|Win32">
	<Configuration>Debug</Configuration>
	<Platform>Win32</Platform>
  </ProjectConfiguration>
  <ProjectConfiguration Include="ARM-Debug|Win32">
	<Configuration>ARM-Debug</Configuration>
	<Platform>Win32</Platform>
  </ProjectConfiguration>
  <ProjectConfiguration Include="ARM-Release|Win32">
	<Configuration>ARM-Release</Configuration>
	<Platform>Win32</Platform>
  </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
	<ProjectGuid>${UUID}</ProjectGuid>
	<Keyword>MakeFileProj</Keyword>
	<ProjectName>${NAME}</ProjectName>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>false</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>true</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Release|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>false</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Debug|Win32'" Label="Configuration">
	<ConfigurationType>Makefile</ConfigurationType>
	<UseDebugLibraries>false</UseDebugLibraries>
	<PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings">
  </ImportGroup>
  <ImportGroup Label="Shared">
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
	<Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
	<Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='ARM-Debug|Win32'">
	<Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='ARM-Release|Win32'">
	<Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Release|Win32'">
	<NMakeBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja</NMakeBuildCommandLine>
	<NMakeReBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja</NMakeReBuildCommandLine>
	<NMakeCleanCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean</NMakeCleanCommandLine>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Debug|Win32'">
	<NMakeBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja</NMakeBuildCommandLine>
	<NMakeReBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja</NMakeReBuildCommandLine>
	<NMakeCleanCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean</NMakeCleanCommandLine>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Debug|Win32'">
	<NMakeBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja</NMakeBuildCommandLine>
	<NMakeReBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja</NMakeReBuildCommandLine>
	<NMakeCleanCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean</NMakeCleanCommandLine>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='ARM-Release|Win32'">
	<NMakeBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja</NMakeBuildCommandLine>
	<NMakeReBuildCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja</NMakeReBuildCommandLine>
	<NMakeCleanCommandLine>\$(SolutionDir)\\toolchain\install.exe &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean</NMakeCleanCommandLine>
  </PropertyGroup>
  <ItemDefinitionGroup>
  </ItemDefinitionGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets">
  </ImportGroup>
</Project>
EOF
  unix2dos --add-bom "$FILE" &> /dev/null
}
