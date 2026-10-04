#!/usr/bin/env python3
"""Fast behavioral lock/inventory/receipt/cleanup regressions, no SDK matrix."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'scripts'))
from cpkt_inventory import load, components_for, record
from cpkt_receipts import publish, read, tree_identity, file_identity, validate_core, component_input_id, group_outputs
from cpkt_operation import run, delegated


class Isolation(unittest.TestCase):
    def setUp(self):
        self.environment={key:value for key,value in os.environ.items() if not key.startswith('CPKT_OPERATION_')}
        (ROOT / 'build').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='isolation-regression-', dir=ROOT / 'build')
        self.root = Path(self.temp.name)
        (self.root / 'cmake').mkdir()
        shutil.copy(ROOT / 'cmake/components.json', self.root / 'cmake/components.json')
        (self.root / 'scripts').mkdir()
        for name in ('cpkt_operation.py','cpkt_inventory.py','cpkt_receipts.py','cpkt_helper_proof.py','cpkt_helper_dispatch.py','cpkt_cmake_inputs.py','cpkt_clangd_check.py'):
            shutil.copy(ROOT / 'scripts' / name, self.root / 'scripts' / name)

    def tearDown(self):
        self.temp.cleanup()

    def command(self, group, *arguments, timeout='0.3', env=None):
        return subprocess.run([sys.executable, str(self.root / 'scripts/cpkt_operation.py'),
            '--root', str(self.root), '--group', group, '--timeout', timeout, '--', *arguments],
            capture_output=True, text=True, env=env if env is not None else self.environment)

    def test_compiler_and_default_temporary_files_stay_in_repository(self):
        code='import os,tempfile,pathlib; p=tempfile.NamedTemporaryFile(delete=False); p.close(); print(p.name); pathlib.Path(p.name).unlink()'
        result=self.command('db',sys.executable,'-c',code)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertTrue(Path(result.stdout.strip()).is_relative_to(self.root/'build/control/tmp'))
        root=self.root/'build/control';(root/'tmp').rename(root/'real-tmp');(root/'tmp').symlink_to(root/'real-tmp',target_is_directory=True)
        result=self.command('db',sys.executable,'-c',code)
        self.assertNotEqual(result.returncode,0);self.assertIn('temporary ancestry',result.stderr)

    def test_dist_cleanup_preserves_database_and_build_state(self):
        module_spec=importlib.util.spec_from_file_location('dist_cleanup_backend',ROOT/'scripts/group-build.py')
        module=importlib.util.module_from_spec(module_spec);module_spec.loader.exec_module(module)
        state=self.root/'build/devenv/data';state.mkdir(parents=True)
        (state/'database').write_bytes(b'important database state')
        dist=self.root/'dist';dist.mkdir();(dist/'package').write_bytes(b'obsolete package')
        before=tree_identity(self.root/'build')
        with patch.object(module,'ROOT',self.root),patch.object(module,'command') as command:
            module.clean('all',dist_only=True)
        self.assertFalse(dist.exists())
        self.assertEqual(before,tree_identity(self.root/'build'))
        command.assert_not_called()

    def test_hardening_configure_revokes_only_its_owned_proof(self):
        shutil.copy(ROOT/'scripts/cpkt_configure_guard.py',self.root/'scripts/cpkt_configure_guard.py')
        target='x86_64-linux-gnu'
        receipts=self.root/'build/verification'/target/'core';receipts.mkdir(parents=True)
        for configuration in ('Debug','Valgrind','Fuzz'):
            for suffix in ('development','built'):
                (receipts/(configuration+'-'+suffix+'.json')).write_bytes(b'existing proof')
        for configuration in ('Valgrind','Fuzz','Debug'):
            before={path.name:path.read_bytes() for path in receipts.iterdir()}
            result=self.command('core',sys.executable,str(self.root/'scripts/cpkt_configure_guard.py'),
                '--root',str(self.root),'--binary',str(self.root/'build'/target/'core'/configuration),
                '--group','core','--target',target,'--configuration','Debug')
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            after={path.name:path.read_bytes() for path in receipts.iterdir()}
            self.assertEqual(after,{name:value for name,value in before.items() if not name.startswith(configuration+'-')})

    def test_inventory_closure_and_ownership(self):
        data = load(self.root)
        for section in ('groups','components','targets','tests'):
            for item in data[section].values():
                for field in ('recipe_inputs','verification_inputs','source_inputs','command_inputs','helper_inputs'):
                    for name in item.get(field,[]):
                        if '${' not in name:
                            self.assertTrue((ROOT/name).is_file(), 'missing inventory input: '+name)
        core = components_for(data, 'core', True)
        self.assertNotIn('postgresql', core)
        self.assertNotIn('whisper', core)
        db = components_for(data, 'db', True)
        self.assertIn('cmocka', db)
        self.assertNotIn('open62541', db)
        misc = components_for(data, 'misc', True)
        self.assertIn('curl', misc)
        self.assertNotIn('postgresql', misc)
        self.assertEqual('core', record(data['targets'], 'cpkt_cmocka_behavior_shared')['group'])
        data['components']['openssl']['dependencies'] = ['sqlite']
        (self.root / 'cmake/components.json').write_text(json.dumps(data))
        with self.assertRaisesRegex(RuntimeError, 'forbidden group edge'):
            load(self.root)

    def test_clangd_exact_header_closure_reuse(self):
        binary=self.root/'build/x86_64-linux-gnu/core/Debug';binary.mkdir(parents=True)
        include=self.root/'include with spaces';include.mkdir()
        header=include/'fixture.h';header.write_text('#define FIXTURE_VALUE 1\n')
        source=self.root/'fixture.c';source.write_text('#include "fixture.h"\nint fixture = FIXTURE_VALUE;\n')
        (binary/'compile_commands.json').write_text(json.dumps([{'directory':str(binary),'file':str(source),
            'arguments':[shutil.which('cc'),'-I'+str(include),'-MMD','-MF','forbidden.d','-o','forbidden.o','-c',str(source)]}]))
        counter=self.root/'counter'
        checker=self.root/'checker.py'
        checker.write_text('#!'+sys.executable+'\nfrom pathlib import Path\np=Path('+repr(str(counter))+')\np.write_text(p.read_text()+"x" if p.exists() else "x")\n')
        checker.chmod(0o755)
        command=[sys.executable,str(self.root/'scripts/cpkt_clangd_check.py'),'--root',str(self.root),
            '--build',str(binary),'--group','core','--source',str(source),'--checker',str(checker),'--gate',str(checker)]
        runner=self.root/'hover-run.py'
        runner.write_text('import os,subprocess\nfrom pathlib import Path\n'
            'fds=tuple(int(os.environ[k]) for k in ("CPKT_OPERATION_FD","CPKT_OPERATION_CAP_FD"))\n'
            'command='+repr(command)+'\n'
            'subprocess.run(command,check=True,pass_fds=fds)\n'
            'subprocess.run(command,check=True,pass_fds=fds)\n'
            'header=Path('+repr(str(header))+')\nstat=header.stat()\nheader.write_text("#define FIXTURE_VALUE 2\\n")\n'
            'os.utime(header,ns=(stat.st_atime_ns,stat.st_mtime_ns))\n'
            'subprocess.run(command,check=True,pass_fds=fds)\n')
        result=self.command('core',sys.executable,str(runner))
        self.assertEqual(0,result.returncode,result.stdout+result.stderr)
        self.assertEqual('xx',counter.read_text())
        self.assertTrue(list((binary/'clangd-proofs').glob('*/*.json')))
        self.assertFalse((self.root/'build/control/helper-proofs').exists())
        self.assertFalse((self.root/'build/clangd').exists())
        self.assertFalse((binary/'forbidden.d').exists())
        self.assertFalse((binary/'forbidden.o').exists())
        result=self.command('core',*command)
        self.assertEqual(0,result.returncode,result.stdout+result.stderr)
        self.assertEqual('xxx',counter.read_text())

    def test_built_does_not_supply_readiness(self):
        with self.assertRaisesRegex(RuntimeError, 'make test GROUP=core PRESET=debug'):
            validate_core(self.root, 'x86_64-linux-gnu', 'Debug', 'debug')

    def test_required_test_inventory_cannot_disappear(self):
        target='x86_64-linux-gnu'
        directory=self.root/'build'/target/'core/Debug'
        directory.mkdir(parents=True)
        (directory/'CMakeCache.txt').write_text('CPKT_TARGET_ID:INTERNAL='+target+'\nCMAKE_BUILD_TYPE:STRING=Debug\n')
        receipt=self.root/'build/verification'/target/'core/Debug-development.json'
        receipt.parent.mkdir(parents=True)
        receipt.write_text(json.dumps({'schema_version':1,'status':'passed','kind':'development',
            'coverage':['required'],'group':'core','target':target,'configuration':'Debug'}))
        with self.assertRaisesRegex(RuntimeError,'required test inventory is absent'):
            validate_core(self.root,target,'Debug','debug')

    def test_configured_preset_resolution_and_selector_rejection(self):
        from configured_build import binary_dir
        query=[sys.executable,str(ROOT/'scripts/cpkt_inventory_cli.py')]
        result=subprocess.run(query,env=dict(self.environment,GROUP='db'),capture_output=True,text=True,check=True)
        self.assertEqual({'postgresql','sqlite','iodbc'},set(json.loads(result.stdout)['components']))
        result=subprocess.run(query+['--group','misc'],env=dict(self.environment,GROUP='db'),capture_output=True,text=True)
        self.assertEqual(0,result.returncode,result.stderr)
        self.assertEqual({'libpng','libharu','miniaudio','whisper','open62541'},set(json.loads(result.stdout)['components']))
        shutil.copy(ROOT/'CMakePresets.json',self.root/'CMakePresets.json')
        directory=self.root/'build/x86_64-linux-gnu/db/Release'
        directory.mkdir(parents=True)
        cache=directory/'CMakeCache.txt'
        cache.write_text('CPKT_TARGET_ID:INTERNAL=x86_64-linux-gnu\nCPKT_GROUP:STRING=db\n')
        with patch.dict(os.environ,{'GROUP':'db','PRESET':'release'},clear=True):
            self.assertEqual(directory,binary_dir(self.root,'x86_64-linux-gnu'))
            with self.assertRaisesRegex(RuntimeError,'resolves to'):
                binary_dir(self.root,'aarch64-linux-gnu')
            cache.write_text('CPKT_TARGET_ID:INTERNAL=aarch64-linux-gnu\n')
            with self.assertRaisesRegex(RuntimeError,'configured target does not match'):
                binary_dir(self.root,'x86_64-linux-gnu')
        events=ROOT/'build/control/events.jsonl'
        before=events.read_bytes() if events.is_file() else None
        for arguments,environment in ((['scripts/fuzz.sh','smoke','--group','db'],self.environment),
                (['scripts/fuzz.sh','smoke','--unknown'],self.environment),
                ([sys.executable,'scripts/group-build.py','build','--group','db','--target','cpkt_pdf_shared'],self.environment),
                ([sys.executable,'scripts/group-build.py','clean','--group','db','--dist-only'],self.environment)):
            result=subprocess.run(arguments if arguments[0]==sys.executable else ['bash',*arguments],
                                  cwd=ROOT,env=environment,capture_output=True,text=True)
            self.assertNotEqual(0,result.returncode)
        self.assertEqual(before,events.read_bytes() if events.is_file() else None)

    def test_public_graph_omissions_link_edges_and_mock_privacy(self):
        for name in ('CpktGroups.cmake','CpktOperation.cmake'):
            shutil.copy(ROOT/'cmake'/name,self.root/'cmake'/name)
        shutil.copy(ROOT/'scripts/cpkt_configure_guard.py',self.root/'scripts/cpkt_configure_guard.py')
        data={'schema_version':1,'components':{},
            'groups':{g:{'requires':[] if g=='core' else ['core']} for g in ('core','db','misc')},
            'targets':{name:{'group':owner,'public':public,'kind':'facade'} for name,owner,public in (
                ('cpkt_public','core',True),('cpkt_optional','misc',False),('cpkt_cmocka_mock','core',False))},
            'tests':{'unused':{'group':'core','execution':'compile','requires':[]}}}
        (self.root/'cmake/components.json').write_text(json.dumps(data))
        (self.root/'probe.c').write_text('int probe(void) { return 0; }\n')
        prefix='''cmake_minimum_required(VERSION 3.21)
include(cmake/CpktGroups.cmake)
include(cmake/CpktOperation.cmake)
project(graph C)
set(CPKT_BUILD_TESTS OFF)
add_custom_target(cpkt_operation_guard)
'''
        cases=[('', 'Missing required public inventory target'),
            ('add_library(cpkt_public STATIC probe.c)\nadd_library(cpkt_optional STATIC probe.c)\ntarget_link_libraries(cpkt_public PUBLIC cpkt_optional)\n','Forbidden group linkage'),
            ('add_library(cpkt_public STATIC probe.c)\nadd_library(cpkt_cmocka_mock STATIC probe.c)\ntarget_link_libraries(cpkt_public PUBLIC cpkt_cmocka_mock)\n','links cmocka'),
            ('add_library(cpkt_public STATIC probe.c)\n',None)]
        for index,(body,failure) in enumerate(cases):
            (self.root/'CMakeLists.txt').write_text(prefix+body+'cmake_language(DEFER CALL cpkt_validate_owned_graph)\n')
            result=self.command('core','cmake','-S',str(self.root),'-B',str(self.root/'build'/str(index)),
                                '-DCPKT_GROUP=core')
            if failure:
                self.assertNotEqual(0,result.returncode)
                self.assertIn(failure,result.stdout+result.stderr)
            else:
                self.assertEqual(0,result.returncode,result.stdout+result.stderr)
        result=self.command('db','cmake','-S',str(self.root),'-B',
            str(self.root/'build/x86_64-linux-gnu/core/Debug'),'-DCPKT_GROUP=db',
            '-DCPKT_TARGET_ID=x86_64-linux-gnu')
        self.assertNotEqual(0,result.returncode)
        self.assertIn('owned by core',result.stdout+result.stderr)

    def test_effective_contract_closure_ignores_unrelated_inputs(self):
        data={'schema_version':1,'groups':{g:{'requires':[] if g=='core' else ['core']} for g in ('core','db','misc')},
              'components':{},'targets':{},'tests':{}}
        recipe='function(cpkt_common)\n  set(value one)\nendfunction()\n'
        for name,group in [('a','core'),('b','core'),('c','db')]:
            data['components'][name]={'group':group,'directory':name,'dependencies':['a'] if name=='c' else [],
                'helpers':['cpkt_add_'+name]+(['cpkt_common'] if name!='b' else []),'recipe_inputs':[],'variants':['static','shared']}
            recipe+='function(cpkt_add_'+name+')\n  set(value one)\nendfunction()\n'
            recipe+='cpkt_prepare_dependency_component(\n NAME '+name+'\n VARIABLES '+name.upper()+'_PIN\n RECIPE_FUNCTIONS cpkt_add_'+name+')\n'
        (self.root/'cmake/components.json').write_text(json.dumps(data))
        path=self.root/'cmake/CpktDependencies.cmake';path.write_text(recipe)
        top=self.root/'CMakeLists.txt';top.write_text('set(A_PIN one)\nset(B_PIN one)\nset(C_PIN one)\n')
        for group in ('core','db'):
            cache=self.root/'build/synthetic'/group/'producer/CMakeCache.txt';cache.parent.mkdir(parents=True)
            cache.write_text('CMAKE_GENERATOR:STRING=Ninja\nCPKT_DEPENDENCY_BUILD_JOBS:STRING=2\n')
        identities=lambda:{name:component_input_id(self.root,'synthetic',name) for name in ('a','b','c')}
        before=identities()
        (self.root/'README.md').write_text('unrelated docs')
        for cache in self.root.glob('build/synthetic/*/producer/CMakeCache.txt'):
            cache.write_text('CMAKE_GENERATOR:STRING=Unix Makefiles\nCPKT_DEPENDENCY_BUILD_JOBS:STRING=1\n')
        self.assertEqual(before,identities())
        runtimes=[]
        for variable,name in [('CPKT_CXX_STDLIB_STATIC_LIBRARY','libstdc++.a'),('CPKT_CXX_LIBGCC_STATIC_LIBRARY','libgcc.a')]:
            runtime=self.root/name;runtime.write_bytes(b'original '+name.encode());runtimes.append(runtime)
            for cache in self.root.glob('build/synthetic/*/producer/CMakeCache.txt'):
                with cache.open('a') as stream:stream.write(variable+':FILEPATH='+str(runtime)+'\n')
        before=identities()
        for runtime in runtimes:
            original=runtime.stat();runtime.write_bytes(b'changed '+runtime.name.encode())
            os.utime(runtime,ns=(original.st_atime_ns,original.st_mtime_ns))
            changed=identities()
            for name in before:self.assertNotEqual(before[name],changed[name])
            before=changed
        top.write_text(top.read_text().replace('C_PIN one','C_PIN two'))
        changed=identities()
        self.assertEqual(before['a'],changed['a']);self.assertEqual(before['b'],changed['b'])
        self.assertNotEqual(before['c'],changed['c'])
        path.write_text(recipe.replace('set(value one)','set(value two)',1))
        helpers=identities()
        self.assertNotEqual(changed['a'],helpers['a']);self.assertNotEqual(changed['c'],helpers['c'])
        self.assertEqual(changed['b'],helpers['b'])
        # Core verification fixtures reading the shared recipe file must retain
        # this same relevant closure rather than hash the entire file.
        from cpkt_receipts import verification_inputs
        data['tests']['core_recipe']={'group':'core','execution':'compile',
            'requires':[],'command_inputs':['cmake/CpktDependencies.cmake']}
        (self.root/'cmake/components.json').write_text(json.dumps(data))
        configured={'CPKT_TARGET_ID':'synthetic','CMAKE_BUILD_TYPE':'Debug'}
        before=verification_inputs(self.root,'core',configured)
        path.write_text(path.read_text().replace('function(cpkt_add_c)\n  set(value one)',
                                                'function(cpkt_add_c)\n  set(value unrelated)'))
        self.assertEqual(before,verification_inputs(self.root,'core',configured))
        path.write_text(path.read_text().replace('function(cpkt_add_a)\n  set(value one)',
                                                'function(cpkt_add_a)\n  set(value relevant)'))
        self.assertNotEqual(before,verification_inputs(self.root,'core',configured))

    def test_output_content_symlink_and_partial_records(self):
        install = self.root / 'install'
        install.mkdir()
        (install / 'lib.a').write_bytes(b'original')
        (install / 'lib.so').symlink_to('lib.a')
        expected = tree_identity(install)
        before = (install / 'lib.a').stat()
        (install / 'lib.a').write_bytes(b'modified')
        os.utime(install / 'lib.a', ns=(before.st_atime_ns,before.st_mtime_ns))
        self.assertNotEqual(expected, tree_identity(install))
        (install / 'lib.so').unlink()
        (install / 'lib.so').symlink_to('missing')
        with self.assertRaisesRegex(RuntimeError, 'dangling'):
            tree_identity(install)
        path = self.root / 'partial.json'
        path.write_text('{"schema_version":1,"status":"running"}')
        with self.assertRaises(RuntimeError):
            read(path)
        path.write_text('{')
        with self.assertRaises(RuntimeError):
            read(path)

    def test_group_export_metadata_and_linker_alias_outputs(self):
        directory=self.root/'binary';directory.mkdir()
        library=directory/'libprobe.so.0';library.write_bytes(b'library')
        alias=directory/'libprobe.so';alias.symlink_to(library.name)
        exports=directory/'cpkt-facades.cmake';exports.write_text('original import definition')
        (directory/'cpkt-owned-outputs.txt').write_text(str(library)+'\n'+str(alias)+'\n')
        expected=group_outputs(directory)
        self.assertIn('cpkt-facades.cmake',expected)
        stat=exports.stat();exports.write_text('modified import definition')
        os.utime(exports,ns=(stat.st_atime_ns,stat.st_mtime_ns))
        self.assertNotEqual(expected,group_outputs(directory))
        alias.unlink()
        with self.assertRaisesRegex(RuntimeError,'outputs missing/corrupt'):
            group_outputs(directory)

    def test_live_delegation_nested_and_scope_rejection(self):
        check = str(self.root / 'scripts/cpkt_operation.py')
        result = self.command('db', sys.executable, check, '--root', str(self.root), '--group', 'db', '--check')
        self.assertEqual(0, result.returncode, result.stderr)
        result = self.command('db', sys.executable, check, '--root', str(self.root), '--group', 'core', '--check')
        self.assertNotEqual(0, result.returncode)

    def test_reopened_fd_and_separate_source_context(self):
        check = str(self.root/'scripts/cpkt_operation.py')
        reopened = self.root/'reopened.py'
        reopened.write_text('import os,sys\nsys.path.insert(0,'+repr(str(self.root/'scripts'))+')\n'
            'from cpkt_operation import delegated\n'
            'fd=os.open('+repr(str(self.root/'build/control/operation.lock'))+',os.O_RDWR)\n'
            'os.environ["CPKT_OPERATION_FD"]=str(fd)\n'
            'delegated('+repr(str(self.root))+',"all")\n')
        result = self.command('all',sys.executable,str(reopened))
        self.assertNotEqual(0,result.returncode)
        self.assertIn('not the owning lock description',result.stderr)
        extracted = self.root/'extracted'
        extracted.mkdir(); (extracted/'CMakeLists.txt').write_text('# separate source context\n')
        child=self.root/'child.py'
        child.write_text('import os,sys\nsys.path.insert(0,'+repr(str(self.root/'scripts'))+')\n'
            'from cpkt_operation import delegated\n'
            'delegated('+repr(str(extracted))+',"all")\n'
            'assert os.environ["CPKT_OPERATION_ROOT"]=='+repr(str(extracted))+'\n')
        result=self.command('all',sys.executable,check,'--root',str(self.root),'--group','all',
            '--source-root',str(extracted),'--',sys.executable,str(child))
        self.assertEqual(0,result.returncode,result.stderr)
        self.assertNotEqual((self.root/'build/control/operation.lock').stat().st_ino,
                            (extracted/'build/control/operation.lock').stat().st_ino)
        result = self.command('all', sys.executable, check, '--root', str(self.root), '--group', 'core', '--',
                              sys.executable, check, '--root', str(self.root), '--group', 'db', '--check')
        self.assertNotEqual(0, result.returncode)
        env = dict(os.environ, CPKT_OPERATION_FD='999', CPKT_OPERATION_ROOT=str(self.root),
                   CPKT_OPERATION_SCOPE='db', CPKT_OPERATION_RUN='fake')
        result = self.command('db', sys.executable, '-c', 'raise SystemExit(0)', env=env)
        self.assertNotEqual(0, result.returncode)
        tamper=self.root/'tamper.py'
        tamper.write_text('import os,sys\nsys.path.insert(0,'+repr(str(self.root/'scripts'))+')\n'
            'from cpkt_operation import delegated\nos.environ["CPKT_OPERATION_SCOPE"]="all"\n'
            'delegated('+repr(str(self.root))+',"all")\n')
        result=self.command('all',sys.executable,check,'--root',str(self.root),'--group','core','--',sys.executable,str(tamper))
        self.assertNotEqual(0,result.returncode)
        self.assertIn('scope string does not match',result.stderr)

    def test_stable_inode_bounded_wait_and_owner_interruption(self):
        script = self.root / 'hold.py'
        script.write_text('import os,time\nfrom pathlib import Path\nPath("' + str(self.root / 'ready') + '").write_text("yes")\ntime.sleep(2)\n')
        owner = subprocess.Popen([sys.executable, str(self.root / 'scripts/cpkt_operation.py'),
                    '--root', str(self.root), '--group', 'all', '--', sys.executable, str(script)],env=self.environment)
        try:
            deadline = time.monotonic()+2
            while not (self.root / 'ready').exists() and time.monotonic()<deadline:
                time.sleep(0.01)
            lock = self.root / 'build/control/operation.lock'
            inode = lock.stat().st_ino
            result = self.command('db', sys.executable, '-c', 'raise SystemExit(0)')
            self.assertNotEqual(0,result.returncode)
            self.assertIn('owner:',result.stderr)
            self.assertEqual(inode,lock.stat().st_ino)
            owner.terminate()
            owner.wait()
            # The child still owns the inherited description after interruption.
            result = self.command('core', sys.executable, '-c', 'raise SystemExit(0)',timeout='0.05')
            self.assertNotEqual(0,result.returncode)
            time.sleep(2)
            result = self.command('db', sys.executable, '-c', 'raise SystemExit(0)')
            self.assertEqual(0,result.returncode,result.stderr)
            self.assertEqual(inode,lock.stat().st_ino)
        finally:
            if owner.poll() is None:
                owner.terminate()
                owner.wait()

    def test_cmake_descriptor_inheritance_and_direct_rejection(self):
        source = self.root / 'source'
        source.mkdir()
        (source / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.21)\nproject(lock NONE)\n'
            'execute_process(COMMAND "' + sys.executable + '" "' + str(self.root / 'scripts/cpkt_operation.py') +
            '" --root "' + str(self.root) + '" --group db --check RESULT_VARIABLE status)\n'
            'if(NOT status EQUAL 0)\nmessage(FATAL_ERROR "delegation missing")\nendif()\n')
        result = self.command('db', 'cmake','-S',str(source),'-B',str(self.root/'binary'))
        self.assertEqual(0,result.returncode,result.stdout+result.stderr)
        result = subprocess.run(['cmake','-S',str(source),'-B',str(self.root/'direct')],capture_output=True,text=True)
        self.assertNotEqual(0,result.returncode)

    def test_helper_dedup_same_run_and_changed_input(self):
        fixture = self.root / 'fixture'
        fixture.write_text('original')
        counter = self.root / 'count'
        runner = self.root / 'run.py'
        proof = self.root / 'scripts/cpkt_helper_proof.py'
        runner.write_text('import subprocess,sys\nfrom pathlib import Path\n'
            'cmd=' + repr([sys.executable,str(proof),'--root',str(self.root),'--group','core','--mode','fixture','--input',str(fixture),'--',sys.executable,'-c',
                          'from pathlib import Path;p=Path('+repr(str(counter))+');p.write_text(p.read_text()+"x" if p.exists() else "x")']) + '\n'
            'import os\nfds=tuple(int(os.environ[key]) for key in ("CPKT_OPERATION_FD","CPKT_OPERATION_CAP_FD"))\n'
            'subprocess.run(cmd,check=True,pass_fds=fds)\nsubprocess.run(cmd,check=True,pass_fds=fds)\n'
            'Path(' + repr(str(fixture)) + ').write_text("changed")\nsubprocess.run(cmd,check=True,pass_fds=fds)\n')
        result = self.command('core',sys.executable,str(runner))
        self.assertEqual(0,result.returncode,result.stdout+result.stderr)
        self.assertEqual('xx',counter.read_text())
        result = self.command('core',sys.executable,str(runner))
        self.assertEqual(0,result.returncode,result.stderr)
        self.assertEqual('xxx',counter.read_text())

    def test_early_helper_ctest_and_direct_execution(self):
        data=load(self.root)
        data['tests']['exact_helper']={'group':'core','execution':'helper','command_inputs':['tests/fixture.py'],
            'requires':[],'preflight':True,'target_sensitive':False,'helper_environment':['PATH']}
        (self.root/'cmake/components.json').write_text(json.dumps(data))
        (self.root/'tests').mkdir()
        counter=self.root/'counter'
        fixture=self.root/'tests/fixture.py'
        fixture.write_text('from pathlib import Path\np=Path('+repr(str(counter))+')\np.write_text(p.read_text()+"x" if p.exists() else "x")\n')
        dispatch=[sys.executable,str(self.root/'scripts/cpkt_helper_dispatch.py'),'--root',str(self.root),
            '--group','core','--target','synthetic','--test','exact_helper','--binary',str(self.root/'binary'),
            '--',sys.executable,str(fixture)]
        source=self.root/'source';source.mkdir()
        (source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.21)\nproject(helper NONE)\ninclude(CTest)\n'
            'add_test(NAME exact_helper COMMAND '+' '.join('"'+value+'"' for value in dispatch)+')\n')
        subprocess.run(['cmake','-S',str(source),'-B',str(self.root/'binary')],check=True,capture_output=True)
        runner=self.root/'helper-runner.py'
        runner.write_text('import os,subprocess\nfds=tuple(int(os.environ[key]) for key in ("CPKT_OPERATION_FD","CPKT_OPERATION_CAP_FD"))\n'
            'subprocess.run('+repr(dispatch)+',check=True,pass_fds=fds)\n'
            'subprocess.run('+repr(['ctest','--test-dir',str(self.root/'binary'),'--no-tests=error'])+',check=True,pass_fds=fds)\n')
        result=self.command('core',sys.executable,str(runner))
        self.assertEqual(0,result.returncode,result.stdout+result.stderr)
        self.assertEqual('x',counter.read_text())
        subprocess.run(['ctest','--test-dir',str(self.root/'binary'),'--no-tests=error'],check=True,capture_output=True,env=self.environment)
        self.assertEqual('xx',counter.read_text())


if __name__ == '__main__':
    unittest.main()
