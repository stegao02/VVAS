SUMMARY = "Vitis AI UNILOG"
DESCRIPTION = "Xilinx Vitis AI components - a wrapper for glog. Define unified log formats for vitis ai tools."

require recipes-vai/vitis-ai-library/vitisai_2.0.inc

SRC_URI += " \
	file://0001-fix-python-path-for-petalinux.patch \
"

S = "${WORKDIR}/git/tools/Vitis-AI-Runtime/VART/unilog"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

DEPENDS = "glog boost"

PACKAGECONFIG[test] = "-DBUILD_TEST=ON,-DBUILD_TEST=OFF,,"
PACKAGECONFIG[python] = ",,,"

inherit cmake

EXTRA_OECMAKE += "-DCMAKE_BUILD_TYPE=Release"

# unilog contains only one shared lib and will therefore become subject to renaming
# by debian.bbclass. Prevent renaming in order to keep the package name consistent
AUTO_LIBNAME_PKGS = ""

FILES_SOLIBSDEV = ""
INSANE_SKIP_${PN} += "dev-so"
FILES_${PN} += "${libdir}/*.so"
