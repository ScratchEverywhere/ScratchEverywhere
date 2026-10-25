"""Create a small offline Scratch 3 project for Xbox runtime/input testing."""
import hashlib
import json
from pathlib import Path
import zipfile

out = Path(__file__).resolve().parent / 'Xbox-Test.sb3'
assets = {}
def costume(name, svg, x, y):
    data = svg.encode()
    asset = hashlib.md5(data).hexdigest()
    assets[asset + '.svg'] = data
    return dict(name=name, assetId=asset, md5ext=asset+'.svg', dataFormat='svg', rotationCenterX=x, rotationCenterY=y)
backdrop = costume('Xbox test', '<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="#153c66"/><path d="M0 180H480M240 0V360" stroke="#527693" stroke-width="2"/><rect x="10" y="10" width="460" height="340" rx="12" fill="none" stroke="#85c8ff" stroke-width="4"/></svg>', 240, 180)
sprite = costume('Orange square', '<svg xmlns="http://www.w3.org/2000/svg" width="50" height="50"><rect x="2" y="2" width="46" height="46" rx="9" fill="#ff9f1c" stroke="white" stroke-width="4"/></svg>', 25, 25)
def block(op, nxt=None, parent=None, inputs=None, fields=None, top=False):
    b=dict(opcode=op, next=nxt, parent=parent, inputs=inputs or {}, fields=fields or {}, shadow=False, topLevel=top)
    if top: b.update(x=20,y=20)
    return b
num=lambda n:[1,[4,str(n)]]
blocks={
 'flag':block('event_whenflagclicked','home',top=True),
 'home':block('motion_gotoxy','loop','flag',{'X':num(-150),'Y':num(0)}),
 'loop':block('control_forever',None,'home',{'SUBSTACK':[2,'move']}),
 'move':block('motion_movesteps','bounce','loop',{'STEPS':num(2)}),
 'bounce':block('motion_ifonedgebounce','wait','move'),
 'wait':block('control_wait',None,'bounce',{'DURATION':num(.05)}),
}
for i,(key,op,arg,amount) in enumerate([('up arrow','motion_changeyby','DY',30),('down arrow','motion_changeyby','DY',-30),('a','looks_changesizeby','CHANGE',10),('u','motion_changeyby','DY',30),('h','motion_changeyby','DY',-30)]):
    hat='key'+str(i); action='action'+str(i)
    blocks[hat]=block('event_whenkeypressed',action,fields={'KEY_OPTION':[key,None]},top=True)
    blocks[hat]['x']=300; blocks[hat]['y']=30+i*120
    blocks[action]=block(op,None,hat,{arg:num(amount)})
common=dict(variables={},lists={},broadcasts={},comments={},currentCostume=0,sounds=[],volume=100)
stage=dict(common,isStage=True,name='Stage',blocks={},costumes=[backdrop],layerOrder=0,tempo=60,videoTransparency=50,videoState='on',textToSpeechLanguage=None)
actor=dict(common,isStage=False,name='Xbox test sprite',blocks=blocks,costumes=[sprite],layerOrder=1,visible=True,x=-150,y=0,size=100,direction=90,draggable=False,rotationStyle='left-right')
project=dict(targets=[stage,actor],monitors=[],extensions=[],meta={'semver':'3.0.0','vm':'0.2.0','agent':'Xbox test fixture'})
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('project.json',json.dumps(project))
    for name,data in assets.items(): z.writestr(name,data)
print(out)
