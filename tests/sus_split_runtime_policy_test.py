#!/usr/bin/env python3
import json
from pathlib import Path
import sys
r=Path(sys.argv[1]);d=json.loads((r/'cmake/components.json').read_text())
m=d['components']['whisper']['package'];assert 'lib/cpkt-cxx/*' in m['owned_patterns']
for archive in ('libstdc++.a','libgcc.a'):
 for name in ('cpkt_cmake_sus_facade','cpkt_sus_mixed_cxx'):
  assert '@prefix/lib/cpkt-cxx/'+archive in d['installed_consumers'][name]['link_contains']
 metadata=(r/'cmake/package_metadata.cmake').read_text();assert '/lib/cpkt-cxx/'+archive in metadata
for key in ('CPKT_CXX_STDLIB_STATIC_LIBRARY','CPKT_CXX_LIBGCC_STATIC_LIBRARY'):
 assert key in (r/'scripts/cpkt_packages.py').read_text()
 assert key in (r/'CMakeLists.txt').read_text()
facade=m['facades'][0];assert facade['forbidden_needed']==['libstdc[+][+][.]so[^]]*','libgcc_s[.]so[^]]*']
assert d['installed_consumers']['cpkt_sus_mixed_cxx']['c_final_link'] is True
consumer=(r/'scripts/cpkt_sdk_consumer.py').read_text()
assert 'LINKER_LANGUAGE C' in consumer
assert "if cpp else 'CMAKE_C_COMPILER'" in consumer
assert "arguments=[configured['CMAKE_C_COMPILER']" in consumer
assert 'pkg-config omitted packaged C++ runtime' in consumer
print('SUS owns selected static runtimes; CMake and raw pkg-config consumers assert their delivered closures and use C final links')
