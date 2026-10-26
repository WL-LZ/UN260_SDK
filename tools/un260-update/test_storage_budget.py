#!/usr/bin/env python3
import unittest
from release_manifest import table
from check_storage_budget import budget, ROOT_FREE_FLOOR_KB, APP_FREE_FLOOR_KB, APP_PATH, BOOT_PREFIX, STATIC_PREFIX

class StorageBudget(unittest.TestCase):
    def test_noop(self):
        a={'a':(b'x',0o644)}
        self.assertEqual(budget(a,a)['root_required_kb'],0)
    def test_rounding_mode_and_backup(self):
        old={'a':(b'x',0o644),'keep':(b'k',0o644)}
        new={'a':(b'x',0o755),'b':(bytes(1025),0o644),'keep':old['keep']}
        r=budget(old,new)
        self.assertEqual(r['root_staged_kb'],11)
        self.assertEqual(r['usb_backup_required_kb'],4101)
        self.assertEqual(r['unchanged'],1)
    def test_full_omission_is_not_deletion(self):
        self.assertEqual(budget({'old':(bytes(4096),0o644)}, {})['root_required_kb'],0)
    def test_boundary(self):
        size=(ROOT_FREE_FLOOR_KB-2048-4)*1024
        self.assertEqual(budget({}, {'app':(bytes(size),0o755)})['root_required_kb'],ROOT_FREE_FLOOR_KB)
        self.assertGreater(budget({}, {'app':(bytes(size+1),0o755)})['root_required_kb'],ROOT_FREE_FLOOR_KB)
    def test_reported_space_regression(self):
        # Reproduce the reported 8-file/14834159-byte replacement envelope.
        sizes=[12121800,2596880,38692,30021,36284,4397,4394,1691]
        new={str(i):(bytes(n),0o644) for i,n in enumerate(sizes)}
        self.assertEqual(sum(sizes),14834159)
        self.assertEqual(budget({},new)['root_required_kb'],16571)
        del new['1']  # Offline raw oracle, not a runtime asset.
        self.assertEqual(budget({},new)['root_required_kb'],14030)
        self.assertLess(budget({},new)['root_required_kb'],16516)
    def test_unified_accounts_exact_application_path_separately(self):
        new={APP_PATH:(bytes(12*1024*1024),0o755),
             'usr/local/bin/test_lvgl.extra':(bytes(2*1024*1024),0o644),
             'usr/local/share/lvgl_data/fonts.bin':(bytes(1024*1024),0o644)}
        legacy=budget({},new)
        managed=budget({},new,'unified-app-volume')
        self.assertGreater(legacy['root_required_kb'],ROOT_FREE_FLOOR_KB)
        self.assertEqual(managed['app_staged_kb'],12*1024+4)
        self.assertEqual(managed['root_staged_kb'],3*1024+8)
        self.assertEqual(managed['root_required_kb'],managed['root_staged_kb']+2048)
        self.assertEqual(managed['app_required_kb'],managed['app_staged_kb']+2048)
        self.assertLess(managed['app_required_kb'],APP_FREE_FLOOR_KB)
        self.assertEqual(legacy['usb_backup_required_kb'],managed['usb_backup_required_kb'])
    def test_app_boundary_and_unchanged_app_reserve(self):
        size=(APP_FREE_FLOOR_KB-2048-4)*1024
        self.assertEqual(budget({}, {APP_PATH:(bytes(size),0o755)},'unified-app-volume')['app_required_kb'],APP_FREE_FLOOR_KB)
        self.assertGreater(budget({}, {APP_PATH:(bytes(size+1),0o755)},'unified-app-volume')['app_required_kb'],APP_FREE_FLOOR_KB)
        old={APP_PATH:(b'app',0o755)}
        new={**old,'resource':(b'new',0o644)}
        self.assertEqual(budget(old,new,'unified-app-volume')['app_required_kb'],2048)
        self.assertEqual(budget(old,old,'unified-app-volume')['app_required_kb'],0)
    def test_factory_trial_matches_exact_target_table(self):
        old={APP_PATH:(b'old',0o755)}
        new={APP_PATH:(b'new',0o755),STATIC_PREFIX+'font.bin':(bytes(1025),0o644)}
        r=budget(old,new,'factory-app-volume')
        self.assertEqual(r['target_table_bytes'],len(table(new)))
        self.assertEqual(r['trial_metadata_kb'],(len(table(new))+1023)//1024+32)
        self.assertEqual(r['root_required_kb'],6+r['trial_metadata_kb']+2048)
        self.assertEqual(r['app_required_kb'],5+2048)
        self.assertFalse(budget(old,new,'unified-app-volume')['trial_enabled'])

    def test_owned_static_delete_only_retains_rollback_and_reserves(self):
        new={APP_PATH:(b'app',0o755)}
        old={**new,STATIC_PREFIX+'retired.bin':(bytes(1025),0o644)}
        r=budget(old,new,'factory-app-volume')
        self.assertEqual((r['changed'],r['replaced'],r['deleted'],r['unchanged']),(1,0,1,1))
        self.assertEqual(r['root_file_staged_kb'],0)
        self.assertEqual(r['root_required_kb'],2048+r['trial_metadata_kb'])
        self.assertEqual(r['app_required_kb'],2048)
        self.assertEqual(r['usb_backup_required_kb'],4096+6)

    def test_noop_factory_does_not_start_trial_or_charge_reserves(self):
        files={APP_PATH:(b'app',0o755),BOOT_PREFIX+'old/player':(bytes(8192),0o755)}
        r=budget(files,files,'factory-app-volume')
        self.assertEqual(r['changed'],0)
        self.assertEqual(r['boot_retained_kb'],8)
        for key in ('boot_publication_kb','trial_metadata_kb','root_required_kb','app_required_kb','usb_backup_required_kb'):
            self.assertEqual(r[key],0,key)

    def test_static_delete_still_needs_boot_publication(self):
        files={APP_PATH:(b'app',0o755),BOOT_PREFIX+'old/player':(b'player',0o755)}
        old={**files,STATIC_PREFIX+'retired.bin':(bytes(1024),0o644)}
        r=budget(old,files,'factory-app-volume')
        self.assertEqual(r['root_file_staged_kb'],0)
        self.assertEqual(r['root_staged_kb'],8)
        self.assertEqual(r['root_peak_kb'],8)
        self.assertEqual(r['root_required_kb'],8+r['trial_metadata_kb']+2048)

    def test_boot_old_versions_are_not_static_deletions_or_free_space(self):
        old={BOOT_PREFIX+'old/player':(bytes(8192),0o755)}
        new={APP_PATH:(b'app',0o755),BOOT_PREFIX+'new/player':(bytes(1024),0o755)}
        r=budget(old,new,'factory-app-volume')
        self.assertEqual(r['deleted'],0)
        self.assertEqual(r['boot_retained_kb'],8)
        self.assertEqual(r['root_file_staged_kb'],5)
        self.assertEqual(r['root_peak_kb'],13)
        self.assertEqual(r['usb_backup_required_kb'],4096)

    def test_manifest_omission_retires_only_owned_metadata(self):
        manifest=STATIC_PREFIX+'locale_fonts/manifest.json'
        app={APP_PATH:(b'app',0o755)}
        old={**app,manifest:(bytes(364504),0o644),'etc/ui_state/customer':(b'user',0o644)}
        replacement={**app,manifest:(bytes(364505),0o644)}
        before=budget(old,replacement,'factory-app-volume')
        after=budget(old,app,'factory-app-volume')
        self.assertEqual(before['root_file_staged_kb']-after['root_file_staged_kb'],360)
        self.assertEqual((after['changed'],after['deleted']),(1,1))
        self.assertEqual(after['usb_backup_required_kb'],4096+360)
        self.assertEqual(after['root_required_kb'],2048+after['trial_metadata_kb'])

    def test_unknown_route_rejected(self):
        with self.assertRaises(ValueError):budget({}, {}, 'ignore-capacity')

if __name__=='__main__': unittest.main()
