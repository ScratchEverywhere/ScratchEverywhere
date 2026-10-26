"""Check a KVM capture or native framebuffer against the color-effect fixture.

Usage: python3 tests/color_effects_pixels.py capture.jpg fixture.expected.json
Use --viewport X Y WIDTH HEIGHT for an overscan-adjusted native framebuffer.
Requires Pillow. Writes capture.pixels.json and returns nonzero on a mismatch.
"""
from PIL import Image
from pathlib import Path
import argparse,sys,json,statistics
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('capture',type=Path)
parser.add_argument('expected',type=Path)
parser.add_argument('--viewport',nargs=4,type=int,metavar=('X','Y','WIDTH','HEIGHT'))
args=parser.parse_args()
path=args.capture;expected=json.loads(args.expected.read_text());im=Image.open(path).convert('RGB');results=[]
if im.size not in [(720,480),(640,480)]:parser.error('Expected a 720x480 KVM capture or 640x480 native framebuffer')
vx,vy,vw,vh=args.viewport or (0,0,*im.size)
if vx<0 or vy<0 or vw<=0 or vh<=0 or vx+vw>im.width or vy+vh>im.height:parser.error('Viewport must fit inside the image')
for item in expected['samples']:
 x,y=round(vx+item['x']*vw/720),round(vy+item['y']*vh/480);pixels=[im.getpixel((x+dx,y+dy)) for dx,dy in [(0,0),(-1,0),(1,0),(0,-1),(0,1)]]
 rgb=[int(statistics.median(c)) for c in zip(*pixels)];delta=max(abs(a-b) for a,b in zip(rgb,item['expected']));results.append(dict(item,actual=rgb,delta=delta,passed=delta<=item['tolerance']))
report={'image':str(path),'image_size':im.size,'viewport':[vx,vy,vw,vh],'passed':sum(r['passed'] for r in results),'total':len(results),'maximum_channel_error':max(r['delta'] for r in results),'failures':[r for r in results if not r['passed']],'samples':results}
path.with_suffix('.pixels.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:report[k] for k in ['passed','total','maximum_channel_error','failures']},indent=2));sys.exit(0 if report['passed']==report['total'] else 1)
