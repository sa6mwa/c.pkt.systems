#!/usr/bin/env python3
"""Exercise the real group backend with small local producers, both generators."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
(ROOT / 'build').mkdir(exist_ok=True)


def invoke(root, *arguments, success=True, env=None):
    result = subprocess.run([sys.executable, str(root / 'scripts/group-build.py'), *arguments],
                            capture_output=True, text=True, env=env)
    if bool(result.returncode == 0) != success:
        raise RuntimeError(' '.join(arguments) + '\n' + result.stdout + result.stderr)
    return result


with tempfile.TemporaryDirectory(prefix='producer-reuse-',dir=ROOT/'build') as temporary:
    for generator in ('Ninja', 'Unix Makefiles'):
        root = Path(temporary) / generator.replace(' ','-')
        (root / 'scripts').mkdir(parents=True)
        (root / 'cmake').mkdir()
        (root / 'tests').mkdir()
        for path in ROOT.glob('scripts/cpkt_*.py'):
            shutil.copy(path, root/'scripts'/path.name)
        shutil.copy(ROOT/'scripts/group-build.py',root/'scripts/group-build.py')
        for name in ('CpktGroups.cmake','CpktOperation.cmake','CpktDependencyContract.cmake'):
            shutil.copy(ROOT/'cmake'/name,root/'cmake'/name)
        # Synthetic-only discovery has no provisioning side effects or external
        # toolchain writes. The real pinned cmocka fixture covers actual tools.
        resolver=root/'scripts/cpkt-toolchains.sh'
        resolver.write_text('#!/bin/sh\nprintf "status=ready\\nsource=synthetic-fixture\\n"\n')
        resolver.chmod(0o755)
        (root/'cmake/CpktReadOnlyToolchain.cmake').write_text('# synthetic native graph\n')
        (root/'main.c').write_text('int main(void) { return 0; }\n')
        components={}
        targets={}
        tests={}
        for group in ('core','db','misc'):
            component=group+'dep'
            components[component]={'group':group,'directory':component,'dependencies':[] if group=='core' else ['coredep'],
                'recipe_functions':['cpkt_add_'+component], 'helpers':['cpkt_add_'+component],
                'recipe_inputs':['tests/phase.py'], 'variants':['static','shared'],'payload':{}}
            targets['cpkt_'+group+'_probe']={'group':group,'kind':'executable','public':False,'source_inputs':['main.c']}
            tests[group+'_behavior']={'group':group,'execution':'runtime','command_inputs':['main.c'],'requires':[]}
        for name,item in json.loads((ROOT/'cmake/components.json').read_text())['targets'].items():
            if item['kind']=='cmake-dashboard':targets[name]=item
        inventory={'schema_version':1,'groups':{g:{'requires':[] if g=='core' else ['core'],'core_components':[],
            'verification_inputs':['main.c']} for g in ('core','db','misc')},'components':components,'targets':targets,'tests':tests}
        (root/'cmake/components.json').write_text(json.dumps(inventory))
        presets={'version':3,'configurePresets':[]}
        for preset,kind in (('debug','Debug'),('release','Release')):
            presets['configurePresets'].append({'name':preset,'generator':generator,'binaryDir':'${sourceDir}/unused',
                'cacheVariables':{'CMAKE_BUILD_TYPE':kind,'CPKT_TARGET_ARCH':'x86_64','CPKT_TARGET_OS':'linux',
                    'CPKT_TARGET_LIBC':'gnu','CPKT_BUILD_TESTS':'ON'}})
        (root/'CMakePresets.json').write_text(json.dumps(presets))
        (root/'tests/phase.py').write_text('''from pathlib import Path
import sys
root=Path(sys.argv[1]);component=sys.argv[2];phase=sys.argv[3]
with (root/'events').open('a') as stream: stream.write(component+':'+phase+'\\n')
if phase=='install':
    output=root/'.cache/deps/x86_64-linux-gnu'/component/'install'
    output.mkdir(parents=True,exist_ok=True)
    (output/'static.a').write_text(component)
    (output/'shared.so').write_text(component)
''')
        recipe='include(ExternalProject)\n'
        for group in ('core','db','misc'):
            component=group+'dep'
            recipe+='''function(cpkt_add_NAME)
  if(CPKT_BUILD_DEPENDENCIES)
    ExternalProject_Add(NAME_project
      PREFIX "${CMAKE_SOURCE_DIR}/.cache/deps-build/${CPKT_TARGET_ID}/NAME"
      SOURCE_DIR "${CMAKE_SOURCE_DIR}/tests"
      DOWNLOAD_COMMAND "${CPKT_OPERATION_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/phase.py" "${CMAKE_SOURCE_DIR}" NAME extract
      CONFIGURE_COMMAND "${CPKT_OPERATION_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/phase.py" "${CMAKE_SOURCE_DIR}" NAME configure
      BUILD_COMMAND "${CPKT_OPERATION_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/phase.py" "${CMAKE_SOURCE_DIR}" NAME build
      INSTALL_COMMAND "${CPKT_OPERATION_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/phase.py" "${CMAKE_SOURCE_DIR}" NAME install)
    set_property(GLOBAL APPEND PROPERTY CPKT_DEPENDENCY_TARGETS NAME_project)
  endif()
endfunction()
'''.replace('NAME',component)
        (root/'cmake/CpktDependencies.cmake').write_text(recipe)
        main='''cmake_minimum_required(VERSION 3.21)
include(cmake/CpktGroups.cmake)
include(cmake/CpktOperation.cmake)
project(synthetic LANGUAGES C)
include(CTest)
set(CPKT_TARGET_ID x86_64-linux-gnu CACHE STRING "")
set(CPKT_DEPENDENCY_BUILD_TYPE Release)
set(CPKT_DEPENDENCY_BUILD_ROOT "${CMAKE_SOURCE_DIR}/.cache/deps-build/${CPKT_TARGET_ID}")
set(CPKT_EXTERNAL_ROOT "${CMAKE_SOURCE_DIR}/.cache/deps/${CPKT_TARGET_ID}")
set(CPKT_DEPENDENCY_CONTRACT_ROOT "${CMAKE_SOURCE_DIR}/.cache/dependency-contracts")
set(CPKT_EXTERNAL_ROOT_LIFECYCLE_OWNED ON)
set(CPKT_DEPENDENCY_BUILD_ROOT_LIFECYCLE_OWNED ON)
include(cmake/CpktDependencyContract.cmake)
include(cmake/CpktDependencies.cmake)
'''
        for group in ('core','db','misc'):
            component=group+'dep'
            main+='''if(CPKT_GROUP STREQUAL "GROUP" OR "GROUP" STREQUAL "core")
  cpkt_prepare_dependency_component(
    NAME COMPONENT
    BUILD_ROOT "${CPKT_DEPENDENCY_BUILD_ROOT}/COMPONENT"
    INSTALL_ROOT "${CPKT_EXTERNAL_ROOT}/COMPONENT/install"
    INPUT_FILES "${CMAKE_SOURCE_DIR}/tests/phase.py"
    RECIPE_FUNCTIONS cpkt_add_COMPONENT)
endif()
'''.replace('\"GROUP\"', '\"'+group+'\"').replace('COMPONENT',component)
        main+='''if(CPKT_DEPENDENCY_PRODUCER)
  set(CPKT_ACTIVE_COMPONENTS coredep)
  if(NOT CPKT_GROUP STREQUAL "core")
    list(APPEND CPKT_ACTIVE_COMPONENTS ${CPKT_GROUP}dep)
  endif()
  cpkt_synthetic_producer()
else()
  add_custom_target(cpkt_operation_guard COMMAND "${CPKT_OPERATION_PYTHON}"
    "${CMAKE_SOURCE_DIR}/scripts/cpkt_operation.py" --root "${CMAKE_SOURCE_DIR}" --group "${CPKT_GROUP}" --check)
  cpkt_group_add_executable(cpkt_${CPKT_GROUP}_probe main.c)
  cpkt_group_add_test(NAME ${CPKT_GROUP}_behavior COMMAND cpkt_${CPKT_GROUP}_probe)
  cmake_language(DEFER CALL cpkt_validate_owned_graph)
endif()
'''
        begin = main.index('if(CPKT_GROUP STREQUAL')
        end = main.index('if(CPKT_DEPENDENCY_PRODUCER)', begin)
        real_recipe = (ROOT/'cmake/CpktDependencies.cmake').read_text()
        loop = real_recipe[real_recipe.index('  set(_all_dependency_targets "")'):real_recipe.rindex('endfunction()')]
        # Exercise the production registration/publication loop with local EPs.
        recipe += 'function(cpkt_synthetic_producer)\n' + loop + 'endfunction()\n'
        (root/'cmake/CpktDependencies.cmake').write_text(recipe + main[begin:end])
        main = main[:begin] + main[end:]
        (root/'CMakeLists.txt').write_text(main)
        env=dict(os.environ)
        for key in list(env):
            if key.startswith('CPKT_OPERATION_'):del env[key]
        # Core bootstraps itself; built-only evidence cannot satisfy db.
        invoke(root,'build','--group','core','--preset','debug',env=env)
        before=(root/'events').read_text()
        failed=invoke(root,'build','--group','db','--preset','debug',success=False,env=env)
        assert 'make test GROUP=core PRESET=debug' in failed.stderr
        assert before==(root/'events').read_text()
        direct=subprocess.run([sys.executable,str(root/'scripts/cpkt_operation.py'),'--group','db','--',
            'cmake','--preset','debug','-B',str(root/'build/x86_64-linux-gnu/db/direct-unready'),
            '-DCPKT_GROUP=db','-DCPKT_TARGET_ID=x86_64-linux-gnu'],cwd=root,env=env,text=True,capture_output=True)
        assert direct.returncode and 'prerequisites failed before project/toolchain' in direct.stderr
        assert 'compiler identification' not in direct.stdout
        invoke(root,'test','--group','core','--preset','debug',env=env)
        before=(root/'events').read_text()
        invoke(root,'test','--group','db','--preset','debug',env=env)
        after=(root/'events').read_text()
        assert after[len(before):].splitlines()==['dbdep:extract','dbdep:configure','dbdep:build','dbdep:install']
        core_receipt=root/'build/verification/x86_64-linux-gnu/core/Debug-development.json'
        core_identity=core_receipt.read_bytes()
        misc_state=root/'build/x86_64-linux-gnu/misc/Debug/sentinel'
        misc_state.parent.mkdir(parents=True);misc_state.write_text('unchanged')
        # Fresh consumer, distinct outer generator, no dependency work.
        before=(root/'events').read_text()
        invoke(root,'build','--group','core','--preset','release',env=env)
        assert before==(root/'events').read_text()
        invoke(root,'test','--group','db','--preset','debug',env=env)
        assert before==(root/'events').read_text()
        assert core_identity==core_receipt.read_bytes()
        assert misc_state.read_text()=='unchanged'
        # All-group target routing builds only its owner, with a read-only core.
        invoke(root,'build','--group','all','--preset','debug','--target','cpkt_db_probe',env=env)
        assert before==(root/'events').read_text()
        assert core_identity==core_receipt.read_bytes()
        assert misc_state.read_text()=='unchanged'
        # Direct/standalone failed reconfiguration must revoke previous success.
        original_project=(root/'CMakeLists.txt').read_text()
        (root/'CMakeLists.txt').write_text(original_project+'\nmessage(FATAL_ERROR "intentional configure failure")\n')
        invoke(root,'configure','--group','core','--preset','debug',success=False,env=env)
        assert not core_receipt.exists()
        (root/'CMakeLists.txt').write_text(original_project)
        invoke(root,'test','--group','core','--preset','debug',env=env)
        core_identity=core_receipt.read_bytes()
        # Content corruption is rejected even if the timestamp is restored.
        artifact=root/'.cache/deps/x86_64-linux-gnu/coredep/install/static.a'
        original=artifact.read_bytes();stat=artifact.stat()
        artifact.write_bytes(b'corrupt');os.utime(artifact,ns=(stat.st_atime_ns,stat.st_mtime_ns))
        before=(root/'events').read_text()
        failed=invoke(root,'build','--group','db','--preset','debug',success=False,env=env)
        assert 'outputs missing/corrupt' in failed.stderr
        assert before==(root/'events').read_text()
        assert core_identity==core_receipt.read_bytes()
        invoke(root,'test','--group','core','--preset','debug',env=env)
        assert artifact.read_bytes()==original
        assert (root/'events').read_text()[len(before):].splitlines()==['coredep:extract','coredep:configure','coredep:build','coredep:install']
        before=(root/'events').read_text()
        # Required reruns invalidate prior success even when the binary is intact.
        original_main = (root/'main.c').read_text()
        (root/'main.c').write_text('int main(void) { return 1; }\n')
        invoke(root,'test','--group','core','--preset','debug',success=False,env=env)
        assert not core_receipt.exists()
        failed=invoke(root,'build','--group','db','--preset','debug',success=False,env=env)
        assert 'make test GROUP=core PRESET=debug' in failed.stderr
        (root/'main.c').write_text(original_main)
        invoke(root,'test','--group','core','--preset','debug',env=env)
        # A changed core test/source definition revokes optional readiness before mutation.
        (root/'main.c').write_text('int main(void) { return 0; } /* changed core verification */\n')
        invoke(root,'build','--group','db','--preset','debug',success=False,env=env)
        assert before==(root/'events').read_text()
        # All-group dependency preparation may explicitly coordinate core
        # readiness, then only the missing optional producer closure.
        invoke(root,'deps','--group','all','--preset','debug',env=env)
        assert (root/'events').read_text()[len(before):].splitlines()==['miscdep:extract','miscdep:configure','miscdep:build','miscdep:install']
        # Group cleanup leaves core, sibling state and persistent control inode.
        inode=(root/'build/control/operation.lock').stat().st_ino
        invoke(root,'clean','--group','db',env=env)
        assert core_receipt.exists() and misc_state.exists()
        assert not (root/'.cache/deps/x86_64-linux-gnu/dbdep').exists()
        assert inode==(root/'build/control/operation.lock').stat().st_ino
        invoke(root,'clean','--group','all',env=env)
        assert inode==(root/'build/control/operation.lock').stat().st_ino
        assert sorted(path.name for path in (root/'build').iterdir()) == ['control']
        assert sorted(path.name for path in (root/'build/control').iterdir()) == ['operation.lock']
        assert not (root/'scripts/__pycache__').exists()
        print(generator+': producer/import, failed prerequisite, fresh Debug/Release consumer, test execution and cleanup passed')
