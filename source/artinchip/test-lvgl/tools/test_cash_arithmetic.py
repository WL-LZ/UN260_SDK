from pathlib import Path
from fractions import Fraction
import ctypes,random,subprocess,tempfile
r=Path(__file__).resolve().parents[1];limit=999999999999999;scale=1000000
with tempfile.TemporaryDirectory(prefix='un260-arithmetic-') as directory:
 lib=Path(directory)/'cash.so';subprocess.run(['cc','-shared','-fPIC','-I.','un260/workspace/calculator_math.c','-o',str(lib)],cwd=r,check=True)
 f=ctypes.CDLL(str(lib)).calculator_calculate;f.argtypes=[ctypes.c_int64,ctypes.c_char,ctypes.c_int64,ctypes.POINTER(ctypes.c_int64)];f.restype=ctypes.c_int
 rng=random.Random(260)
 for i in range(5000):
  a=rng.randint(-limit,limit);b=rng.randint(-limit,limit);op=rng.choice('+-*/');out=ctypes.c_int64(731)
  if i<16:a=[0,1,-1,limit][i%4];b=[0,scale,-scale,limit][i//4]
  status=f(a,op.encode(),b,ctypes.byref(out))
  if op=='/' and b==0:assert status==4;continue
  exact=Fraction(a+b) if op=='+' else Fraction(a-b) if op=='-' else Fraction(a*b,scale) if op=='*' else Fraction(a*scale,b)
  positive=abs(exact);q,rem=divmod(positive.numerator,positive.denominator);q+=rem*2>=positive.denominator;expected=-q if exact<0 else q
  if abs(expected)>limit:assert status==3
  else:assert status==0 and out.value==expected,(a,op,b,status,out.value,expected)
 print('PASS 5,000 deterministic cases against exact rational arithmetic, including rounding and overflow')
