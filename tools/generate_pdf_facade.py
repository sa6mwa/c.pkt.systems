#!/usr/bin/env python3
"""Generate the C89 libHaru facade from the pinned upstream public headers.

Usage: python3 tools/generate_pdf_facade.py PATH_TO_LIBHARU_INCLUDE
The generated header and source are checked in so downstream builds need no Python.
"""

import pathlib
import re
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
STRUCTS = {
    "HPDF_Point", "HPDF_Point3D", "HPDF_Rect", "HPDF_Box", "HPDF_Date",
    "HPDF_TextWidth", "HPDF_DashMode", "HPDF_TransMatrix",
    "HPDF_3DMatrix", "HPDF_RGBColor", "HPDF_CMYKColor",
}


def c89_text(value):
    value = re.sub(r"/\*.*?\*/", "", value, flags=re.S)
    value = re.sub(r"//[^\n]*", "", value)
    return value.replace("HPDF_", "CPKT_PDF_")


def snake(name):
    name = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    name = re.sub(r"([A-Z])([A-Z][a-z])", r"\1_\2", name)
    return name.lower().replace("__", "_")


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    upstream = pathlib.Path(sys.argv[1])
    types = (upstream / "hpdf_types.h").read_text()
    consts = (upstream / "hpdf_consts.h").read_text()
    api = (upstream / "hpdf.h").read_text()
    enum_types = set(re.findall(r"typedef enum[^;]*}\s*(HPDF_\w+)\s*;", types, re.S))
    functions = re.findall(
        r"HPDF_EXPORT\(([^)]*)\)\s*(HPDF_\w+)\s*\((.*?)\)\s*;", api, re.S
    )
    if len(functions) != 290:
        raise SystemExit("unexpected libHaru API: expected 290 exports, got %d" % len(functions))
    types = types[types.index("/*  native OS integer types") : types.rfind("#ifdef __cplusplus")]
    types = c89_text(types)
    types = types.replace("typedef  signed long long    CPKT_PDF_INT64;",
        "typedef struct { unsigned int lo; signed int hi; } CPKT_PDF_INT64;")
    types = types.replace("typedef  unsigned long long  CPKT_PDF_UINT64;",
        "typedef struct { unsigned int lo; unsigned int hi; } CPKT_PDF_UINT64;")
    if "long long" in types:
        raise SystemExit("non-C89 64-bit type in generated public types")
    consts = consts[consts.index("#define  HPDF_TRUE") : consts.rfind("#endif")]
    consts = c89_text(consts)
    handles = re.findall(r"typedef HPDF_HANDLE\s+(HPDF_\w+);", api)

    header = [
        "/* Generated from libHaru 2.4.6 public headers; see tools/generate_pdf_facade.py. */",
        "/* libHaru license: share/doc/c.pkt.systems/third_party/libharu/LICENSE. */",
        "#ifndef CPKT_PDF_H", "#define CPKT_PDF_H", "#include <stdlib.h>",
        "#if defined(__GNUC__) || defined(__clang__)",
        '#define CPKT_PDF_API __attribute__((visibility("default")))',
        "#else", "#define CPKT_PDF_API", "#endif",
        "#ifdef __cplusplus", 'extern "C" {', "#endif",
        "#define CPKT_PDF_STDCALL", "", types, "", consts,
        "typedef void *CPKT_PDF_HANDLE;",
    ]
    header.extend("typedef CPKT_PDF_HANDLE CPKT_PDF_%s;" % h[5:] for h in handles)
    header.append("")
    source = [
        "/* Generated from libHaru 2.4.6 public headers; see tools/generate_pdf_facade.py. */",
        '#include "cpkt/pdf.h"', "#define HPDF_SHARED 1", "#include <hpdf.h>", "#include <string.h>",
        "typedef char cpkt_pdf_uint32_is_32_bits[(sizeof(unsigned int) == 4) ? 1 : -1];",
    ]
    for struct in sorted(STRUCTS):
        source.append("typedef char cpkt_pdf_%s_layout[(sizeof(%s) == sizeof(CPKT_PDF_%s)) ? 1 : -1];" % (
            struct.lower(), struct, struct[5:]))
    source.append("")
    exported_names = []
    for ret, original, args in functions:
        ret = ret.strip()
        args = re.sub(r"/\*.*?\*/", "", args, flags=re.S)
        params = [p.strip() for p in args.split(",")] if args.strip() != "void" else []
        name = "cpkt_pdf_" + snake(original[5:])
        exported_names.append(name)
        public_ret = ret.replace("HPDF_", "CPKT_PDF_")
        public_params = [p.replace("HPDF_", "CPKT_PDF_") for p in params]
        comment = "/** Calls libHaru's %s with C89 facade types. */" % original
        header.append(comment)
        header.append("CPKT_PDF_API %s %s(%s);" % (public_ret, name, ", ".join(public_params) or "void"))
        source.append(comment)
        source.append("%s %s(%s)" % (public_ret, name, ", ".join(public_params) or "void"))
        source.append("{")
        declarations, before, after, native_args = [], [], [], []
        for p in params:
            match = re.search(r"([A-Za-z_]\w*)\s*$", p)
            if not match:
                raise SystemExit("cannot parse %s argument: %s" % (original, p))
            arg = match.group(1)
            tokens = re.findall(r"HPDF_\w+", p)
            kind = tokens[0] if tokens else ""
            if kind == "HPDF_UINT64":
                native_args.append("(((HPDF_UINT64)%s.hi << 32) | %s.lo)" % (arg, arg))
            elif kind in STRUCTS and "*" not in p:
                declarations.append("    %s native_%s;" % (kind, arg))
                before.append("    memcpy(&native_%s, &%s, sizeof(native_%s));" % (arg, arg, arg))
                native_args.append("native_%s" % arg)
            elif kind in STRUCTS and "*" in p:
                declarations.append("    %s native_%s;" % (kind, arg))
                native_args.append("%s ? &native_%s : NULL" % (arg, arg))
                after.append("    if (%s) memcpy(%s, &native_%s, sizeof(*%s));" % (arg, arg, arg, arg))
            elif kind in enum_types:
                native_args.append("(%s)%s" % (kind, arg))
            else:
                native_args.append(arg)
        call = "%s(%s)" % (original, ", ".join(native_args))
        if ret in STRUCTS:
            declarations.extend(["    %s native_result;" % ret, "    %s result;" % public_ret])
            source.extend(declarations)
            source.extend(before)
            source.append("    native_result = %s;" % call)
            source.append("    memcpy(&result, &native_result, sizeof(result));")
            source.extend(after)
            source.append("    return result;")
        elif ret == "void":
            source.extend(declarations)
            source.extend(before)
            source.append("    %s;" % call)
            source.extend(after)
        elif after:
            declarations.append("    %s result;" % ret)
            source.extend(declarations)
            source.extend(before)
            source.append("    result = %s;" % call)
            source.extend(after)
            source.append("    return result;")
        else:
            source.extend(declarations)
            source.extend(before)
            if ret in enum_types:
                source.append("    return (%s)%s;" % (public_ret, call))
            else:
                source.append("    return %s;" % call)
        source.extend(["}", ""])
    header.extend(["", "#ifdef __cplusplus", "}", "#endif", "#endif", ""])
    (ROOT / "include/cpkt/pdf.h").write_text("\n".join(header))
    (ROOT / "src/pdf.c").write_text("\n".join(source))
    (ROOT / "cmake/exports/cpkt_pdf.txt").write_text(
        "# Defined dynamic exports of libcpkt_pdf. Keep this list exact.\n"
        + "\n".join(sorted(exported_names)) + "\n"
    )
    formatter = shutil.which("clang-format")
    if formatter is None:
        raise SystemExit("clang-format is required to regenerate the PDF facade")
    subprocess.run(
        [formatter, "-i", str(ROOT / "include/cpkt/pdf.h"), str(ROOT / "src/pdf.c")],
        check=True,
    )
    print("generated %d libHaru wrappers" % len(functions))


if __name__ == "__main__":
    main()
