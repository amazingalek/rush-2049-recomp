# Dot-source from PowerShell: `. tools/env.ps1`. Same setup as tools/env.sh.
$N64TC = "$env:LOCALAPPDATA\Programs\n64-toolchain"
$vc    = "$N64TC\portable-msvc\msvc\VC\Tools\MSVC\14.44.35207"
$sdk   = "$N64TC\portable-msvc\msvc\Windows Kits\10"
$sdkv  = "10.0.26100.0"

$env:PATH    = "$N64TC\llvm\bin;$N64TC\cmake\bin;$N64TC\ninja;$N64TC\ccache;$N64TC\make\bin;$vc\bin\Hostx64\x64;$sdk\bin\$sdkv\x64;$env:PATH"
$env:INCLUDE = "$vc\include;$sdk\Include\$sdkv\ucrt;$sdk\Include\$sdkv\shared;$sdk\Include\$sdkv\um;$sdk\Include\$sdkv\winrt;$sdk\Include\$sdkv\cppwinrt"
$env:LIB     = "$vc\lib\x64;$sdk\Lib\$sdkv\ucrt\x64;$sdk\Lib\$sdkv\um\x64"

# MIPS patches: MSYS2 clang (has the MIPS backend) + llvm.org ld.lld.
$env:MIPS_CC      = "$N64TC\msys64\clang64\bin\clang.exe"
$env:MIPS_LD      = "$N64TC\llvm\bin\ld.lld.exe"
$env:MIPS_OBJDUMP = "$N64TC\msys64\clang64\bin\llvm-objdump.exe"
Remove-Variable vc, sdk, sdkv
