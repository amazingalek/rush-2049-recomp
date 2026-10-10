/* Functions implemented natively in the executable (src/mod_exports.cpp). N64Recomp turns a call to one of these
   dummy addresses into a call to the C++ function of the same name. Game functions need no entry here: N64Recomp
   resolves them by name from syms/rush2049/rush2049.us.syms.toml. */
recomp_vprintf = 0x8F000000;
