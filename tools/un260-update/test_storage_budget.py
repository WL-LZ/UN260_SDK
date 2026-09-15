#!/usr/bin/env python3
import unittest
from check_storage_budget import budget, ROOT_FREE_FLOOR_KB

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

if __name__=='__main__': unittest.main()
