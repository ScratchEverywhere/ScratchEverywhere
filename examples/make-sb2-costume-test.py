"""Generate a Scratch 2 integration test for named/numeric costume changes.

Expected: PASSES=6, ERRORS=0, a blue backdrop and red sprite.
This fixture is not one of the twenty qualification games.
"""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

out=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('SB2-Costume-Test.sb2')
assets={}
def costume(index,name,color,w,h):
    data=('<svg xmlns="http://www.w3.org/2000/svg" width="%s" height="%s"><rect width="100%%" height="100%%" fill="%s"/></svg>'%(w,h,color)).encode()
    assets[str(index)+'.svg']=data
    return dict(costumeName=name,baseLayerID=index,baseLayerMD5=hashlib.md5(data).hexdigest()+'.svg',bitmapResolution=1,rotationCenterX=w/2,rotationCenterY=h/2)
def check(reporter,value):
    return ['doIfElse',['=',reporter,value],[['changeVar:by:','PASSES',1]],[['changeVar:by:','ERRORS',1]]]
stage_script=[['whenGreenFlag'],['setVar:to:','PASSES',0],['setVar:to:','ERRORS',0],
    ['startScene','Play'],check(['sceneName'],'Play'),
    ['startScene','Lost'],check(['sceneName'],'Lost'),
    ['startScene',1],check(['sceneName'],'Play'),
    ['doBroadcastAndWait','test sprite'],['stopScripts','all']]
actor_script=[['whenIReceive','test sprite'],['lookLike:','green'],check(['costumeName'],'green'),
    ['lookLike:','red'],check(['costumeName'],'red'),
    ['lookLike:',2],check(['costumeName'],'green'),['lookLike:','red']]
actor=dict(objName='Actor',variables=[],lists=[],scripts=[[10,10,actor_script]],costumes=[costume(2,'red','#ff0000',30,30),costume(3,'green','#00ff00',30,30)],sounds=[],currentCostumeIndex=0,scratchX=0,scratchY=0,scale=1,direction=90,rotationStyle='normal',isDraggable=False,visible=True)
monitors=[dict(target='Stage',cmd='getVar:',param=n,color=15629590,label=n,mode=1,sliderMin=0,sliderMax=100,isDiscrete=True,x=20,y=20+i*40,visible=True) for i,n in enumerate(['PASSES','ERRORS'])]
project=dict(objName='Stage',variables=[dict(name=n,value=0,isPersistent=False) for n in ['PASSES','ERRORS']],lists=[],scripts=[[10,10,stage_script]],costumes=[costume(0,'Play','#173351',480,360),costume(1,'Lost','#993333',480,360)],sounds=[],currentCostumeIndex=0,penLayerID=-1,tempoBPM=60,videoAlpha=.5,children=[actor]+monitors,info=dict(swfVersion='v461',spriteCount=1,scriptCount=2))
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('project.json',json.dumps(project))
    for name,data in assets.items():z.writestr(name,data)
print(out)
