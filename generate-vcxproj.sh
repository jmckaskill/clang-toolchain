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
		Linux-Arm|ALL = Linux-Arm|ALL
		Windows-Debug|ALL = Windows-Debug|ALL
		Windows-Release|ALL = Windows-Release|ALL
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


write_project() {
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
		$UUID.Linux-Arm|ALL.ActiveCfg = Linux-Arm|Win32
		$UUID.Windows-Debug|ALL.ActiveCfg = Windows-Debug|Win32
		$UUID.Windows-Release|ALL.ActiveCfg = Windows-Release|Win32"

	echo "GENERATING $DIR/$FILE.vcxproj"

	pushd $DIR &> /dev/null
	FILE="$NAME.vcxproj"

	cat > $FILE <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Windows-Release|Win32">
      <Configuration>Windows-Release</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Windows-Debug|Win32">
      <Configuration>Windows-Debug</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Linux-Arm|Win32">
      <Configuration>Linux-Arm</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
  </ItemGroup>
EOF


	if [ -n "$FOLDERS" ]; then
		echo '  <ItemGroup>' >> $FILE
		for file in `find $FOLDERS -iname '*.c' -o -iname '*.cpp'`; do
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
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Windows-Release|Win32'" Label="Configuration">
    <ConfigurationType>Makefile</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Windows-Debug|Win32'" Label="Configuration">
    <ConfigurationType>Makefile</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Linux-Arm|Win32'" Label="Configuration">
    <ConfigurationType>Makefile</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings">
  </ImportGroup>
  <ImportGroup Label="Shared">
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Windows-Release|Win32'">
    <Import Project="\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Windows-Debug|Win32'">
    <Import Project="\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Linux-Arm|Win32'">
    <Import Project="\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Windows-Release|Win32'">
    <NMakeOutput>\$(SolutionDir)\\${TYPE}\\win32-release\\${DEBUG_TARGET}</NMakeOutput>
    <NMakePreprocessorDefinitions>WIN32;NDEBUG</NMakePreprocessorDefinitions>
    <NMakeBuildCommandLine>"c:\\Program Files\\git\\usr\\bin\\sh.exe" --login \$(SolutionDir)\\toolchain\install.sh \$(SolutionDir)\\toolchain &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/win32-release/${BUILD_TARGET}</NMakeBuildCommandLine>
    <NMakeReBuildCommandLine>"c:\\Program Files\\git\\usr\\bin\\sh.exe" --login \$(SolutionDir)\\toolchain\install.sh \$(SolutionDir)\\toolchain &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/win32-release/${BUILD_TARGET} &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/win32-release/${BUILD_TARGET}</NMakeReBuildCommandLine>
    <NMakeCleanCommandLine>"c:\\Program Files\\git\\usr\\bin\\sh.exe" --login \$(SolutionDir)\\toolchain\install.sh \$(SolutionDir)\\toolchain &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/win32-release/${BUILD_TARGET}</NMakeCleanCommandLine>
    <NMakeIncludeSearchPath>\$(SolutionDir);\$(ProjectDir)</NMakeIncludeSearchPath>
    <IntDir>\$(SolutionDir)\\obj\\\$(Configuration)\</IntDir>
    <SourcePath />
    <ExcludePath />
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Windows-Debug|Win32'">
    <NMakeOutput>\$(SolutionDir)\\${TYPE}\\win32-debug\\${DEBUG_TARGET}</NMakeOutput>
    <NMakePreprocessorDefinitions>WIN32</NMakePreprocessorDefinitions>
    <NMakeBuildCommandLine>"c:\\Program Files\\git\\usr\\bin\\sh.exe" --login \$(SolutionDir)\\toolchain\install.sh \$(SolutionDir)\\toolchain &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/win32-debug/${BUILD_TARGET}</NMakeBuildCommandLine>
    <NMakeReBuildCommandLine>"c:\\Program Files\\git\\usr\\bin\\sh.exe" --login \$(SolutionDir)\\toolchain\install.sh \$(SolutionDir)\\toolchain &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/win32-debug/${BUILD_TARGET} &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/win32-debug/${BUILD_TARGET}</NMakeReBuildCommandLine>
    <NMakeCleanCommandLine>"c:\\Program Files\\git\\usr\\bin\\sh.exe" --login \$(SolutionDir)\\toolchain\install.sh \$(SolutionDir)\\toolchain &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/win32-debug/${BUILD_TARGET}</NMakeCleanCommandLine>
    <NMakeIncludeSearchPath>\$(SolutionDir);\$(ProjectDir)</NMakeIncludeSearchPath>
    <IntDir>\$(SolutionDir)\\obj\\\$(Configuration)\</IntDir>
    <SourcePath />
    <ExcludePath />
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Linux-Arm|Win32'">
    <NMakeOutput>\$(SolutionDir)\\${TYPE}\linux-arm\\${DEBUG_TARGET}</NMakeOutput>
    <NMakePreprocessorDefinitions>__linux__;__arm__</NMakePreprocessorDefinitions>
    <NMakeBuildCommandLine>"c:\\Program Files\\git\\usr\\bin\\sh.exe" --login \$(SolutionDir)\\toolchain\install.sh \$(SolutionDir)\\toolchain &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/linux-arm/${BUILD_TARGET}</NMakeBuildCommandLine>
    <NMakeReBuildCommandLine>"c:\\Program Files\\git\\usr\\bin\\sh.exe" --login \$(SolutionDir)\\toolchain\install.sh \$(SolutionDir)\\toolchain &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/linux-arm/${BUILD_TARGET} &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja ${TYPE}/linux-arm/${BUILD_TARGET}</NMakeReBuildCommandLine>
    <NMakeCleanCommandLine>"c:\\Program Files\\git\\usr\\bin\\sh.exe" --login \$(SolutionDir)\\toolchain\install.sh \$(SolutionDir)\\toolchain &amp;&amp; \$(SolutionDir)\\toolchain\\host\\bin\\ninja -C \$(SolutionDir) -f msvc.ninja -t clean ${TYPE}/linux-arm/${BUILD_TARGET}</NMakeCleanCommandLine>
    <NMakeIncludeSearchPath>\$(SolutionDir)/protorpc/include;\$(SolutionDir)/ext/bearssl/inc;\$(SolutionDir)/protorpc/ext;\$(SolutionDir);\$(ProjectDir);\$(SolutionDir)/clang-linux-arm/include/c++;\$(SolutionDir)/clang-linux-arm/include</NMakeIncludeSearchPath>
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
		$UUID.Linux-Arm|ALL.ActiveCfg = Linux-Arm|Win32
		$UUID.Windows-Debug|ALL.ActiveCfg = Windows-Debug|Win32
		$UUID.Windows-Release|ALL.ActiveCfg = Windows-Release|Win32"

	cat > "$FILE" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Windows-Release|Win32">
      <Configuration>Windows-Release</Configuration>
      <Platform>Win32</Platform>
    </ProjectConfiguration>
	<ProjectConfiguration Include="Windows-Debug|Win32">
		<Configuration>Windows-Debug</Configuration>
		<Platform>Win32</Platform>
	</ProjectConfiguration>
	<ProjectConfiguration Include="Linux-Arm|Win32">
		<Configuration>Linux-Arm</Configuration>
		<Platform>Win32</Platform>
	</ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>${UUID}</ProjectGuid>
    <Keyword>MakeFileProj</Keyword>
    <ProjectName>${NAME}</ProjectName>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Windows-Release|Win32'" Label="Configuration">
    <ConfigurationType>Makefile</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Windows-Debug|Win32'" Label="Configuration">
    <ConfigurationType>Makefile</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Linux-Arm|Win32'" Label="Configuration">
    <ConfigurationType>Makefile</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v140</PlatformToolset>
  </PropertyGroup>
  <Import Project="\$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings">
  </ImportGroup>
  <ImportGroup Label="Shared">
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Windows-Release|Win32'">
    <Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Windows-Debug|Win32'">
    <Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <ImportGroup Label="PropertySheets" Condition="'\$(Configuration)|\$(Platform)'=='Linux-Arm|Win32'">
    <Import Project="\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props" Condition="exists('\$(UserRootDir)\\Microsoft.Cpp.\$(Platform).user.props')" Label="LocalAppDataPlatform" />
  </ImportGroup>
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Windows-Release|Win32'">
    <NMakeBuildCommandLine>${COMMAND}</NMakeBuildCommandLine>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Windows-Debug|Win32'">
    <NMakeBuildCommandLine>${COMMAND}</NMakeBuildCommandLine>
  </PropertyGroup>
  <PropertyGroup Condition="'\$(Configuration)|\$(Platform)'=='Linux-Arm|Win32'">
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

