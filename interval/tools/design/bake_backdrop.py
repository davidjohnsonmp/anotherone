"""Bakes the one bitmap in the UI: the studio backdrop, the paper cards and the navy footer. 2x resolution."""
import numpy as np, cv2
from pathlib import Path
S=2; W,H=1396,580
CARDS=[(24,22,260,478),(300,22,300,478),(616,22,440,478),(1072,22,300,478)]
RADIUS=30
FOOT=(24,514,1348,52)
rng=np.random.default_rng(7)
w,h=W*S,H*S
yy,xx=np.mgrid[0:h,0:w].astype(np.float32); u=xx/w; v=yy/h
def srgb(hexs): return np.array([int(hexs[i:i+2],16)/255 for i in (0,2,4)],dtype=np.float32)
# seamless paper roll: brighter pool of light right of centre, falling away to the edges, as in the photos
pool=np.exp(-(((u-.60)/.62)**2+((v-.42)/.72)**2))
img=(srgb('C9D5D9')[None,None]*(1-pool[...,None])+srgb('E9EFF0')[None,None]*pool[...,None])
def blur(a,sig): return cv2.GaussianBlur(a,(0,0),sig*S)
def rrect_mask(x,y,cw,ch,r):
    m=np.zeros((h,w),np.float32); ss=4
    big=np.zeros((ch*S*ss+8,cw*S*ss+8),np.uint8)
    R=r*S*ss; X0,Y0=4,4; X1,Y1=cw*S*ss+3,ch*S*ss+3
    cv2.rectangle(big,(X0+R,Y0),(X1-R,Y1),255,-1); cv2.rectangle(big,(X0,Y0+R),(X1,Y1-R),255,-1)
    for cx,cy in ((X0+R,Y0+R),(X1-R,Y0+R),(X0+R,Y1-R),(X1-R,Y1-R)): cv2.circle(big,(cx,cy),R,255,-1)
    small=cv2.resize(big,(cw*S+2,ch*S+2),interpolation=cv2.INTER_AREA).astype(np.float32)/255
    X,Y=x*S-1,y*S-1; m[Y:Y+small.shape[0],X:X+small.shape[1]]=small
    return m
def shade(img,mask,alpha,col=(0.10,0.13,0.20)):
    return img*(1-(mask*alpha)[...,None])+np.array(col,np.float32)*(mask*alpha)[...,None]
# footer: a capsule of navy suiting with a fine twill, lit from above
fx,fy,fw,fh=FOOT
fm=rrect_mask(fx,fy,fw,fh,fh//2)
twill=np.sin((xx+yy)*2*np.pi/5.0)*.5+.5
navy=srgb('1E2336')[None,None]*(1+.10*(twill[...,None]-.5))
navy=navy*(1.18-.34*np.clip((yy-fy*S)/(fh*S),0,1)[...,None])
img=shade(img,blur(np.roll(fm,8*S,axis=0),11),.24)
img=img*(1-fm[...,None])+navy*fm[...,None]
# shirt and tie at the centre of the capsule
cx=W/2*S; top=fy*S
V=np.array([[cx-40*S,top],[cx+40*S,top],[cx,top+38*S]],np.int32)
shirt=np.zeros((h,w),np.uint8); cv2.fillConvexPoly(shirt,V,255,lineType=cv2.LINE_AA); shirt=shirt.astype(np.float32)/255*fm
collar_grad=(srgb('D3DFEE')[None,None]*(1-np.clip((yy-top)/(38*S),0,1)[...,None]*.32))
stripe=(np.sin((xx+yy)*2*np.pi/2.6)*.5+.5)*.35+.65*np.sin(yy*2*np.pi/2.6)**2
tie=srgb('6F7040')[None,None]*(.84+.32*stripe[...,None])
dots=((np.sin(xx*2*np.pi/7.0)*np.sin(yy*2*np.pi/7.0))>.62).astype(np.float32)
tie=tie*(1-dots[...,None]*.5)+srgb('C8BC7C')[None,None]*dots[...,None]*.5
img=img*(1-shirt[...,None])+collar_grad*shirt[...,None]
tm=np.zeros((h,w),np.uint8)
cv2.ellipse(tm,(int(cx),int(top+7*S)),(int(9*S),int(8*S)),0,0,360,255,-1,cv2.LINE_AA)
blade=np.array([[cx-5.5*S,top+12*S],[cx+5.5*S,top+12*S],[cx+11*S,(fy+fh)*S],[cx-11*S,(fy+fh)*S]],np.int32)
cv2.fillConvexPoly(tm,blade,255,lineType=cv2.LINE_AA); tm=tm.astype(np.float32)/255*fm
img=shade(img,blur(np.roll(tm,1*S,axis=1),1.2)*fm,.28)
img=img*(1-tm[...,None])+tie*tm[...,None]
# paper cards with a soft studio shadow
for x,y,cw,ch in CARDS:
    m=rrect_mask(x,y,cw,ch,RADIUS)
    img=shade(img,blur(np.roll(m,14*S,axis=0),17),.20)
    img=shade(img,blur(np.roll(m,3*S,axis=0),3.5),.15)
    uu=(xx-x*S)/(cw*S); vv=(yy-y*S)/(ch*S)
    t=np.clip(.62*vv+.38*uu,0,1)[...,None]
    paper=srgb('EFF3F5')[None,None]*(1-t)+srgb('DEE6EB')[None,None]*t
    grain=cv2.GaussianBlur(rng.normal(0,1,(h,w)).astype(np.float32),(0,0),.8)[...,None]*.005
    paper=paper+grain
    img=img*(1-m[...,None])+paper*m[...,None]
    # top edge catching the light
    edge=np.clip(m-np.roll(m,1*S,axis=0),0,1)
    img=img*(1-edge[...,None]*.5)+np.array([1,1,1],np.float32)*edge[...,None]*.5
# dither keeps the long gradients free of banding
img=img+(rng.random(img.shape,dtype=np.float32)-rng.random(img.shape,dtype=np.float32))/255*.8
out=(np.clip(img,0,1)*255+.5).astype(np.uint8)[...,::-1]
cv2.imwrite(str(Path(__file__).resolve().parents[2]/'source/assets/img/backdrop.jpg'),out,[cv2.IMWRITE_JPEG_QUALITY,95,cv2.IMWRITE_JPEG_SAMPLING_FACTOR,cv2.IMWRITE_JPEG_SAMPLING_FACTOR_444])
print(out.shape)
