"""Static artwork with real native text, hit areas, knobs and meters."""
from pathlib import Path
import sys,json,math,shutil,hashlib,copy,os
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parents[2];SKIN=R/'skins/SGK - VST - SGK Hyperion';P=SKIN/'Plugin Skins';P.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(Path(__file__).resolve().parent));import Native as N
N.P=P;N.defs=[];WID,HGT=1280,628
source=Image.open(R/'Resources/Gold-Carousel-Clean.png');sx=WID/source.width;sy=HGT/source.height
source.resize((WID,HGT),Image.Resampling.LANCZOS).save(P/'carousel.png')
def rect(x,y,w,h):return tuple(round(v*s)for v,s in zip((x,y,w,h),(sx,sy,sx,sy)))
def cond(p,v,n):return f'IndexedEnabling/{v}/{n}/Parameter {p}'
HOME=[cond(23,0,2),cond(24,0,4)];MENU=[cond(23,1,2)]
GOLD='fff4dfb4';BLACK='ff20180e'
def label(name,p,x,y,w,h,size=24,color=GOLD,conditions=None,style='SemiBold'):
    N.value(name,w,h,size,False,True);d=N.defs[-1]['value']['componentsData'][0]['componentData']['data'];d['textStyle']['colour']=color;d['textStyle']['font']['style']=style
    return N.place(name,name,p,x,y,w,h,conditions)
def hit(name,p,x,y,w,h,conditions=None):
    N.value(name,w,h,20,True);N.defs[-1]['value']['componentsData'].pop(0)
    return N.place(name,name,p,x,y,w,h,conditions)
def film(name,p,x,y,w,h,path,frames,conditions=None,touch=False,horizontal=False):
    data=dict(version=5,knobType='FilmStrip',filmStrip=path,numFrames=frames,invert=False,dragOrientation='Horizontal'if horizontal else'Vertical',handleName='Data')
    kids=[N.component('Knob',name,data,0,0,w,h)];actions=[N.action('Mouse Down','Q-Link')]if touch else[]
    N.defs.append(N.local(name,kids,actions,not touch));node=N.place(name,name,p,x,y,w,h,conditions)
    if touch:node['bounds']['acceptsHWFocus']='Yes'
    return node
def art(name,file,x,y,w,h,conditions=None):return N.image(name,file,x,y,w,h,conditions)
def font(n):return ImageFont.truetype(os.environ.get('HYPERION_FONT','C:/Windows/Fonts/segoeuib.ttf' if os.name=='nt' else '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'),n)
def panel(d,box):d.rounded_rectangle(box,12,fill='#211b12',outline='#c49b57',width=2)
def text(d,xy,s,size=22):d.text(xy,s,font=font(size),fill='#f3ddb5')

kids=[art('Approved gold artwork','carousel.png',0,0,WID,HGT)]
px,py,pw,ph=rect(727,651,187,42);position=Image.new('RGBA',(pw,ph));pd=ImageDraw.Draw(position);pd.rounded_rectangle((0,0,pw-1,ph-1),9,fill='#4d391b',outline='#b68b42');position.save(P/'position-plate.png');kids.append(art('PositionPlate','position-plate.png',px,py,pw,ph,HOME))
lx,ly,lw,lh=rect(465,303,16,16);led=Image.new('RGBA',(lw,lh));ld=ImageDraw.Draw(led);ld.ellipse((1,1,lw-2,lh-2),fill='#a8ef48',outline='#e5ffae',width=1);led.save(P/'ready-led.png');kids.append(art('ReadyLED','ready-led.png',lx,ly,lw,lh,HOME+[cond(14,1,2)]))
# All live labels and their taps share identical exported coordinates.
for name,p,box,size,style in [('CenterName',15,(465,497,700,72),42,'Bold Italic'),('CenterName2',113,(465,561,700,30),21,'SemiBold'),('PrevName',20,(55,490,290,58),27,'SemiBold'),('PrevName2',114,(55,547,290,47),24,'SemiBold'),('NextName',21,(1280,490,300,58),27,'SemiBold'),('NextName2',115,(1280,547,300,47),24,'SemiBold'),('BankPlaque',16,(705,284,233,43),22,'SemiBold'),('Status',14,(487,290,150,35),19,'SemiBold'),('PlayingState',17,(715,590,205,34),18,'SemiBold'),('Position',22,(730,650,180,44),21,'SemiBold'),('BankChoice',16,(510,742,380,60),26,'SemiBold')]:
    kids.append(label(name,p,*rect(*box),size,conditions=HOME,style=style))
for name,p,box in [('PlayCenter',32,(430,266,775,372)),('PreviousCard',3,(46,318,310,290)),('NextCard',4,(1276,318,310,290)),('PreviousArrow',3,(348,393,66,137)),('NextArrow',4,(1227,393,67,137)),('FavoritePreset',13,(1112,282,84,60)),('FavoriteFilter',12,(39,739,83,77)),('Search',10,(165,740,288,72)),('BankSelector',9,(495,739,458,76)),('AllSounds',11,(999,739,330,77))]:kids.append(hit(name,p,*rect(*box),HOME))
# Swipe rail is independent of the card's play button, preventing accidental loads.
x,y,w,h=rect(440,646,760,53);Image.new('RGBA',(w,h*3),(0,0,0,1)).save(P/'swipe.png')
rail=film('PresetSwipe',1,x,y,w,h,'swipe.png',3,HOME,True,True);N.defs[-1]['value']['actions'].append(dict(version=2,onAction='Enter Pressed',handler='Toggle Switch',handleName='Confirm',additionalData='',**{'handle remapping':dict(version=1,map=[])}));rail['handle remapping']['map'].append(dict(key='Confirm',value='Parameter 32'));kids.append(rail)
N.defs[-1]['value']['componentsData'].insert(0,N.component('Focus','Focus',dict(version=1,backgroundColour='00000000',outlineColour='00000000',backgroundInset=2.0,outlineThickness=0.0),0,0,w,h))
# Exact intrinsic dimensions prevent the top-left-only rendering failure.
for index,box in enumerate([(23,157,319,85),(345,157,273,85),(620,157,251,85),(874,157,274,85),(1150,157,219,85),(1371,157,253,85)]):
    x,y,w,h=rect(*box);im=Image.new('RGBA',(w,h*3));d=ImageDraw.Draw(im)
    for frame in range(3):
        if index==0 and frame<2:
            for j in range(h):
                c=45+int(22*(1-j/max(1,h-1)));d.line((0,frame*h+j,w,frame*h+j),fill=(c,int(c*.80),int(c*.51),255))
        elif index>0 and frame==2:
            for j in range(h):
                t=j/max(1,h-1);d.line((0,frame*h+j,w,frame*h+j),fill=(int(216-42*t),int(174-42*t),int(102-40*t),255))
    fn=f'category-{index}.png';im.save(P/fn);kids.append(film('CategoryState'+str(index),80+index,x,y,w,h,fn,3,HOME))
    for value,color in [(1,GOLD),(2,BLACK)]:kids.append(label(f'Category{index}Text{value}',80+index,x+12,y+2,w-24,h-4,26,color,HOME+[cond(80+index,value,3)]))
    left=max(x+12,44 if index==0 else 0);right=min(x+w-12,1238 if index==5 else 1280)
    kids.append(hit('CategoryTap'+str(index),56+index,left,y,right-left,h,HOME))
for name,p,x in [('CategoryBack',38,0),('CategoryNext',39,1238)]:
    im=Image.new('RGBA',(42,52),(0,0,0,0));d=ImageDraw.Draw(im);d.text((10,7),'‹'if p==38 else'›',font=font(29),fill='#fff0c8');fn=name+'.png';im.save(P/fn);kids+=[art(name+'Icon',fn,x,108,42,52,HOME),hit(name,p,x,108,42,52,HOME)]
for side,p,box in [('L',18,(1417,759,190,19)),('R',19,(1417,787,190,19))]:
    x,y,w,h=rect(*box);im=Image.new('RGBA',(w,h*13));d=ImageDraw.Draw(im)
    for frame in range(13):
        for j in range(12):d.rectangle((j*w/12+1,frame*h+1,(j+1)*w/12-1,(frame+1)*h-1),fill='#f1ce78'if j<frame else'#292216')
    fn=f'meter-{side}.png';im.save(P/fn);kids.append(film('Output'+side,p,x,y,w,h,fn,13,HOME))
# Favorite fill is separate from the star's whole-area touch control.
x,y,w,h=rect(1128,283,54,49);im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im);points=[]
for j in range(10):
    a=-math.pi/2+j*math.pi/5;radius=min(w,h)*(.48 if j%2==0 else .22);points.append((w/2+math.cos(a)*radius,h/2+math.sin(a)*radius))
d.polygon(points,fill='#ffe07b');im.save(P/'favorite-filled.png');kids.append(art('FavoriteFill','favorite-filled.png',x,y,w,h,HOME+[cond(90,1,2)]))
# On secondary pages replace the baked HOME selection with a real page indicator.
x,y,w,h=rect(0,868,1635,79);im=Image.new('RGBA',(w,h*4));d=ImageDraw.Draw(im)
for page in range(1,4):
    for yy in range(h):
        shade=20+int(16*(1-yy/h));d.line((0,page*h+yy,w,page*h+yy),fill=(shade,int(shade*.8),int(shade*.5),255))
    cuts=[0,359,651,937,1280]
    for tab,title in enumerate(['HYPERION','SOUND','FX','SETTINGS']):
        left,right=cuts[tab],cuts[tab+1];d.line((left,page*h+4,left,(page+1)*h-4),fill='#ab894f',width=1)
        if tab==page:d.rounded_rectangle((left+8,page*h+5,right-8,(page+1)*h-5),7,fill='#ba9655',outline='#f0ce85',width=2)
        f=font(24);tw=d.textlength(title,font=f);d.text(((left+right-tw)/2,page*h+(h-28)/2),title,font=f,fill='#17110c'if tab==page else'#f6e3bf')
im.save(P/'page-tabs.png');kids.append(film('PageSelection',24,x,y,w,h,'page-tabs.png',4))
# Native whole-area button bindings over the approved bottom tabs.
for name,p,box in [('HomeTab',34,(30,871,430,74)),('SoundTab',35,(467,871,360,74)),('FXTab',36,(830,871,365,74)),('SettingsTab',37,(1200,871,410,74))]:kids.append(hit(name,p,*rect(*box)))

# Full browser overlays; names split into two native labels when needed.
for mode in [1,2,3,4]:
    conditions=[cond(89,mode,5)];im=Image.new('RGBA',(1248,458));d=ImageDraw.Draw(im);panel(d,(0,0,1247,457));text(d,(26,14),'SGK HYPERION',22);panel(d,(1105,8,1234,48));text(d,(1130,16),'CLOSE',19)
    im.save(P/f'menu-{mode}.png');kids.append(art('Menu'+str(mode),f'menu-{mode}.png',16,102,1248,458,conditions));kids.append(label('MenuTitle'+str(mode),25,340,110,290,36,24,conditions=conditions));kids.append(label('MenuPage'+str(mode),26,905,110,170,36,22,conditions=conditions));kids.append(hit('CloseMenu'+str(mode),33,1121,110,129,40,conditions))
    rows=6 if mode==4 else 16
    for row in range(rows):
        per=3 if mode==4 else 8;x=32+(row//per)*613;y=154+(row%per)*(49 if mode==4 else 43)
        rowim=Image.new('RGBA',(600,43));rd=ImageDraw.Draw(rowim);rd.rounded_rectangle((1,1,598,41),7,fill='#352b1a',outline='#886c3f',width=1);fn='row.png';rowim.save(P/fn)
        kids.append(art(f'RowBackground{mode}-{row}',fn,x,y,600,43,conditions))
        kids.append(label(f'RowName{mode}-{row}',40+row,x+10,y,580,26,22,conditions=conditions));kids.append(label(f'RowNameTail{mode}-{row}',96+row,x+10,y+24,580,18,16,conditions=conditions));kids.append(hit(f'RowTap{mode}-{row}',40+row,x,y,600,43,conditions))
    if mode==4:
        kids.append(label('SearchQuery',86,40,309,1110,33,24,conditions=conditions))
        for row,letters in enumerate(['QWERTYUIOP','ASDFGHJKL','ZXCVBNM']):
            x0=50+(10-len(letters))*57
            for col,letter in enumerate(letters):
                x=x0+col*116;y=347+row*49;im=Image.new('RGBA',(106,44));d=ImageDraw.Draw(im);panel(d,(0,0,105,43));d.text((39,5),letter,font=font(26),fill='#f6dfb2');fn=f'key-{letter}.png';im.save(P/fn)
                keyConditions=conditions+([cond(159,0,2)]if row==0 else[])
                kids.append(art('KeyArt'+letter,fn,x,y,106,44,keyConditions));kids.append(hit('KeyTouch'+letter,128+ord(letter.lower())-97,x,y,106,44,keyConditions))
        for col,digit in enumerate('1234567890'):
            x=50+col*116;y=347;im=Image.new('RGBA',(106,44));d=ImageDraw.Draw(im);panel(d,(0,0,105,43));d.text((39,5),digit,font=font(26),fill='#f6dfb2');fn=f'key-{digit}.png';im.save(P/fn);kc=conditions+[cond(159,1,2)];kids.extend([art('DigitArt'+digit,fn,x,y,106,44,kc),hit('DigitTouch'+digit,160+int(digit),x,y,106,44,kc)])
        for name,p,x,w in [('123 / ABC',159,40,90),('CLEAR',156,140,90),('SPACE',154,250,600),('BACK',155,870,190)]:
            im=Image.new('RGBA',(w,39));d=ImageDraw.Draw(im);panel(d,(0,0,w-1,38));d.text((w/2-34,6),name,font=font(15 if w<100 else 20),fill='#f6dfb2');fn=name.replace('/','-')+'.png';im.save(P/fn);kids.extend([art(name+'Art',fn,x,497,w,39,conditions),hit(name+'Touch',p,x,497,w,39,conditions)])
    for name,p,x in [('BACK',30,1065 if mode==4 else 40),('NEXT',31,1160 if mode==4 else 230)]:
        y=503 if mode!=4 else 535;w=170 if mode!=4 else 85;h=40 if mode!=4 else 22
        im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im);d.rounded_rectangle((0,0,w-1,h-1),5,fill='#624a25',outline='#d8b46e');d.text((w/2-25,1 if mode==4 else 7),name,font=font(15 if mode==4 else 20),fill='#fff0cb');fn=f'page-{mode}-{name}.png';im.save(P/fn);kids.extend([art(name+str(mode),fn,x,y,w,h,conditions),hit(name+'Tap'+str(mode),p,x,y,w,h,conditions)])

def knob(name,p,x,y,w=112,h=112,conditions=None):
    frames=33;im=Image.new('RGBA',(w,h*frames));d=ImageDraw.Draw(im)
    for frame in range(frames):
        cy=h*frame+h/2;cx=w/2
        for radius in range(w//2-3,0,-1):
            t=radius/(w/2);c=int(90+85*(1-t));d.ellipse((cx-radius,cy-radius,cx+radius,cy+radius),fill=(c,int(c*.82),int(c*.51),255))
        d.ellipse((cx-w*.47,cy-h*.47,cx+w*.47,cy+h*.47),outline='#e0bc7b',width=2);d.ellipse((cx-w*.42,cy-h*.42,cx+w*.42,cy+h*.42),outline='#241a0e',width=3)
        a=math.radians(-135+270*frame/(frames-1));d.line((cx+math.sin(a)*w*.25,cy-math.cos(a)*h*.25,cx+math.sin(a)*w*.40,cy-math.cos(a)*h*.40),fill='#fff1cc',width=4)
    fn=f'knob-gold-{w}x{h}.png';im.save(P/fn);return film(name,p,x,y,w,h,fn,frames,conditions,True)
for page,title in [(1,'SOUND'),(3,'SETTINGS')]:
    conditions=[cond(23,0,2),cond(24,page,4)];im=Image.new('RGBA',(1248,458));d=ImageDraw.Draw(im);panel(d,(0,0,1247,457));text(d,(36,22),title,32)
    if page==1:text(d,(80,122),'OUTPUT',25);text(d,(420,122),'PAN',25);text(d,(755,122),'PRESET EFFECTS',25)
    elif page==2:
        text(d,(60,140),'Effects follow the selected preset.',27);text(d,(60,190),'This test build keeps the current independent FX models.',22);text(d,(60,238),'Full FX editing will follow the MPC playback test.',22)
    else:
        text(d,(60,142),'Private MPC Key 37 / Hakai test',27);text(d,(60,188),'2,256 PC-validated presets. Source content stays on the drive.',22);text(d,(60,233),'Swipe rail / Q2 previews. Tap center to play.',22);text(d,(60,278),'Arrows load adjacent sounds. Q1 category / Q3 bank.',22);text(d,(60,325),'PANIC clears notes, FX tails and clipping indication.',22)
    im.save(P/f'page-{page}.png');kids.append(art(title+'Panel',f'page-{page}.png',16,102,1248,458,conditions));kids.append(label(title+'Playing',27,60,169,1120,50,32,conditions=conditions))
    if page==1:
        kids += [knob('OutputKnob',5,100,280,conditions=conditions),label('OutputValue',5,65,397,190,40,25,conditions=conditions),knob('PanKnob',7,430,280,conditions=conditions),label('PanValue',7,390,397,190,40,25,conditions=conditions),label('SoundFXValue',6,730,297,440,70,29,conditions=conditions),hit('SoundFXToggle',6,730,285,440,95,conditions)]
    if page==2:kids += [label('FXValue',6,725,414,450,75,29,conditions=conditions),hit('FXToggle',6,725,414,450,75,conditions)]
    if page==3:
        kids += [label('StatusMessage',28,70,478,780,45,21,conditions=conditions)]
    im=Image.new('RGBA',(200,47));d=ImageDraw.Draw(im);panel(d,(0,0,199,46));text(d,(50,9),'PANIC',22);im.save(P/'panic.png');kids += [art('PanicArt'+str(page),'panic.png',1000,500,200,47,conditions),hit('PanicTouch'+str(page),8,1000,500,200,47,conditions)]
# FX rack uses native hit areas and live parameter values, with static gold panels.
FX=[cond(23,0,2),cond(24,2,4)]
OV=FX+[cond(229,0,2)];ED=FX+[cond(229,1,2)]
im=Image.new('RGBA',(1248,458));d=ImageDraw.Draw(im);panel(d,(0,0,1247,457));text(d,(26,14),'PRESET FX RACK',28);im.save(P/'fx-rack-panel.png');kids.append(art('RackPanel','fx-rack-panel.png',16,102,1248,458,FX))
def button(name,p,title,x,y,w,h,conditions):
    im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im)
    for yy in range(h):
        c=int(80-28*yy/max(1,h-1));d.line((1,yy,w-2,yy),fill=(c,int(c*.77),int(c*.45),255))
    d.rounded_rectangle((1,1,w-2,h-2),8,outline='#d7ad68',width=2)
    size=19;f=font(size);d.text(((w-d.textlength(title,font=f))/2,(h-size)/2-2),title,font=f,fill='#fff0cf')
    fn=name+'.png';im.save(P/fn);kids.extend([art(name+'Art',fn,x,y,w,h,conditions),hit(name+'Touch',p,x,y,w,h,conditions)])
kids.append(label('RackPlaying',27,570,118,650,36,24,conditions=FX))
kids.append(label('RackHelp',232,60,158,650,32,21,conditions=OV))
button('RackGlobal',6,'PRESET FX',960,155,240,38,OV);kids.append(label('RackGlobalState',6,730,157,225,32,19,conditions=OV))
for row in range(8):
    x=40+(row//4)*615;y=202+(row%4)*72
    im=Image.new('RGBA',(600,64));d=ImageDraw.Draw(im)
    for yy in range(64):
        c=int(57-20*yy/63);d.line((0,yy,599,yy),fill=(c,int(c*.82),int(c*.60),255))
    d.rounded_rectangle((1,1,598,62),9,outline='#bd9559',width=2);d.line((9,5,590,5),fill='#d8b97c')
    for xx in [10,588]:d.ellipse((xx,28,xx+5,33),fill='#17130d',outline='#9a8155')
    im.save(P/'rack-row.png');kids.append(art('RackRowArt'+str(row),'rack-row.png',x,y,600,64,OV))
    kids.append(label('RackNumber'+str(row),256+row,x+25,y+6,72,50,18,conditions=OV))
    kids.append(label('RackName'+str(row),204+row,x+100,y+5,352,54,24,conditions=OV))
    kids.append(hit('RackSelect'+str(row),212+row,x+18,y+2,450,60,OV))
    kids.append(label('RackOn'+str(row),264+row,x+465,y+5,120,54,18,conditions=OV))
    kids.append(hit('RackBypass'+str(row),242+row,x+465,y+2,120,60,OV))
button('RackBackPage',171,'PREVIOUS',42,505,170,40,OV);button('RackNextPage',172,'NEXT',365,505,170,40,OV)
kids.append(label('RackPage',230,220,505,135,40,22,conditions=OV));button('RackResetAll',179,'RESET ALL FX',1000,505,205,40,OV)
button('RackBack',170,'BACK TO RACK',42,158,205,42,ED);kids.append(label('SelectedEffect',228,265,158,570,42,29,conditions=ED))
button('EffectEnabled',175,'ON / BYPASS',960,158,245,42,ED);kids.append(label('EffectOnState',175,820,158,140,42,20,conditions=ED))
for k in range(6):
    x=62+k*197;c=ED+[cond(236+k,1,2)]
    kids.append(label('FXControlName'+str(k),198+k,x-18,221,190,42,21,conditions=c))
    kids.append(knob('FXControl'+str(k),192+k,x+20,274,112,112,c))
    kids.append(label('FXControlValue'+str(k),192+k,x-18,399,190,44,23,conditions=c))
button('ControlsPrevious',173,'<',42,474,64,42,ED);kids.append(label('ControlPage',231,110,474,100,42,22,conditions=ED));button('ControlsNext',174,'>',215,474,64,42,ED)
kids.append(label('EffectLevelLabel',178,680,494,180,42,22,conditions=ED));kids.append(knob('FXOutputTrim',178,860,459,76,76,ED))
button('EffectReset',176,'RESET EFFECT',980,480,225,48,ED)

N.defs.append(N.local('Carousel',kids))
tui=dict(pageData=dict(version=1,componentDefinitions=dict(version=2,importFiles=[],localComponentDefinitions=N.defs),info=dict(version=1,type='CompleteDescription'),tabs=[dict(version=3,tabName='Hyperion',fnKeyIndex=0,fnKeySubIndex=0,qlinkBoundsData=['0 0 0 0'],componentName='Carousel',initialSize='0 0 1280 628',scale=1.0)]))
(P/'TUI.json').write_text(json.dumps(tui,indent=2))
q={f'Q-Link {i+1}':([0,1,2,5]+[-1]*12)[i]for i in range(16)}
ql={'version':4,'info':{'version':1,'type':'CompleteDescription'},'Screen Mode Q-Links':{'version':4,'map':[{'Tab':1,'SubTab':1,'Bank Direction':'Column','Q-Links':q}]},'Program Mode Q-Links':q}
for f in ['Q-Links.json','Q-Links - 8by1.json']:(P/f).write_text(json.dumps(ql,indent=2))
(SKIN/'plugin-meta.xml').write_text('<PLUGIN name="SGK Hyperion" descriptiveName="SGK Hyperion" format="VST" category="Synth" manufacturer="SGK" version="0.1.2" file="%payload-path%/SGK - VST - SGK Hyperion/sgk_hyperion.so" uid="53474859" isInstrument="1" fileTime="0" infoUpdateTime="0" numInputs="0" numOutputs="2" isShell="0"/>\n')
(SKIN/'version.xml').write_text('<plugincontent version="1.0"><identifier>SGK.vst.sgkhyperion</identifier><version>0.1.2.0</version></plugincontent>')

print("Generated Hyperion native skin",len(N.defs),"definitions")
