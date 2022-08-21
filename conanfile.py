import os

from conan.tools import files
from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMake, CMakeDeps, CMakeToolchain
from conans import tools

required_conan_version = ">=1.50.0"


class ArcusConan(ConanFile):
    name = "pyarcus"
    license = "LGPL-3.0"
    author = "Ultimaker B.V."
    url = "https://github.com/Ultimaker/libArcus"
    description = "Communication library between internal components for Ultimaker software"
    topics = ("conan", "python", "binding", "sip", "cura", "protobuf")
    settings = "os", "compiler", "build_type", "arch"
    revision_mode = "scm"
    exports = "LICENSE*"

    python_requires = "umbase/0.1.5@ultimaker/testing"
    python_requires_extend = "umbase.UMBaseConanfile"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "py_build_requires": ["ANY"],
        "py_build_backend": ["ANY"],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "py_build_requires": '"sip >=6, <7", "setuptools>=40.8.0", "wheel"',
        "py_build_backend": "sipbuild.api",
    }
    scm = {
        "type": "git",
        "subfolder": ".",
        "url": "auto",
        "revision": "auto"
    }

    def requirements(self):
        for req in self._um_data()["requirements"]:
            self.requires(req)

    def config_options(self):
        if self.options.shared and self.settings.compiler == "Visual Studio":
            del self.options.fPIC

    def configure(self):
        self.options["protobuf"].shared = True
        self.options["cpython"].shared = True
        self.options["arcus"].shared = True

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            tools.check_min_cppstd(self, 17)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        if self.settings.compiler == "Visual Studio":
            tc.blocks["generic_system"].values["generator_platform"] = None
            tc.blocks["generic_system"].values["toolset"] = None
        tc.variables["Python_EXECUTABLE"] = self.deps_user_info["cpython"].python.replace("\\", "/")
        tc.variables["Python_USE_STATIC_LIBS"] = not self.options["cpython"].shared
        tc.variables["Python_ROOT_DIR"] = self.deps_cpp_info["cpython"].rootpath.replace("\\", "/")
        tc.variables["Python_FIND_FRAMEWORK"] = "NEVER"
        tc.variables["Python_FIND_REGISTRY"] = "NEVER"
        tc.variables["Python_FIND_IMPLEMENTATIONS"] = "CPython"
        tc.variables["Python_FIND_STRATEGY"] = "LOCATION"
        tc.generate()

    def layout(self):
        cmake_layout(self)

        if self.settings.os in ["Linux", "FreeBSD", "Macos"]:
            self.cpp.package.system_libs = ["pthread"]

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        packager = files.AutoPackager(self)
        packager.patterns.build.lib = ["*.so", "*.so.*", "*.a", "*.lib", "*.dylib", "*.pyd", "*.pyi"]
        packager.run()

        files.files.rmdir(self, os.path.join(self.package_folder, "lib", "pyArcus"))

    def package_info(self):
        if self.in_local_cache:
            self.runenv_info.append_path("PYTHONPATH", os.path.join(self.package_folder, "lib"))
        else:
            self.runenv_info.append_path("PYTHONPATH", self.build_folder)
