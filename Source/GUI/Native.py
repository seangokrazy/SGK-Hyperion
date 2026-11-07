from PIL import Image
P=None
def bounds(x,y,w,h,cond=None):return {'version':2,'acceptsHWFocus':'No','showWhenDataModelInvalid':'Show','whenVisible':'Always','boundsType':'Absolute','bounds':f'{x} {y} {w} {h}','additionalInvalidatingHandles':cond or []}
def component(typ,name,data,x,y,w,h,maps=None,cond=None):return {'version':2,'componentData':{'version':1,'name':name,'type':typ,'data':data},'handle remapping':{'version':1,'map':maps or []},'bounds':bounds(x,y,w,h,cond)}
def image(name,file,x,y,w,h,cond=None):return component('Image',name,{'version':2,'imageType':'Regular','colour':'0','image':file},x,y,w,h,cond=cond)
def place(typ,name,param,x,y,w,h,cond=None):return component(typ,name,{'version':1,'handleName':'Data'},x,y,w,h,[{'key':'Data','value':f'Parameter {param}'}],cond)
def action(on,handler,extra=''):return {'version':2,'onAction':on,'handler':handler,'handleName':'' if handler=='Show Overlay' else 'Data','additionalData':extra,'handle remapping':{'version':1,'map':[]}}
def local(name,kids,acts=None,ignore=True):
 clear={'version':1,'colour':'0','image':''};return {'key':name,'value':{'version':4,'actions':acts or [],'backgroundData':{'version':1,'focussed':clear,'unfocussed':clear},'ignoreMousePresses':ignore,'disableCoarseDataWheel':False,'repeats':1,'hideQLinkBounds':True,'componentsData':kids}}
defs=[]
def value(name,w,h,size=24,toggle=False,readonly=False):
 label=component('Label','Value',{'version':1,'textStyle':{'version':1,'font':{'version':1,'name':'Titillium Web','style':'SemiBold','height':size},'colour':'ffe9edf6','justification':'horizontallyCentred verticallyCentred','case':'Original'},'type':'Value','handleName':'Data'},0,0,w,h);kids=[label]
 if toggle:
  fn=f'hit_{w}_{h}.png';Image.new('RGBA',(w,h),(0,0,0,1)).save(P/fn);kids.append(component('Button','Toggle',{'version':2,'onImage':fn,'offImage':fn,'buttonId':1,'numButtonsInGroup':1,'handleName':'Data','gestureBehaviour':'Instant'},0,0,w,h))
 acts=[] if readonly else [action('Enter Pressed','Toggle Switch')] if toggle else [action('Mouse Down','Show Overlay','knob overlay'),action('Enter Pressed','Show Overlay','knob overlay')]
 defs.append(local(name,kids,acts,readonly))
