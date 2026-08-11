"""Pack the 16-bit denoised heightmap into an RGBA8 image (R=high byte, G=low
byte) for landscape_import_heightmap_from_render_target(rt, InImportHeightFromRGChannel=True)."""
import os, numpy as np
from PIL import Image
T=r"G:\personal_projects\CHANGELING\Tools"
V=np.asarray(Image.open(os.path.join(T,"heightmap_denoised_505.png"))).astype(np.uint16)
print("heightmap %s  V %d..%d"%(V.shape, V.min(), V.max()))
rgba=np.zeros(V.shape+(4,),np.uint8)
rgba[...,0]=(V>>8).astype(np.uint8)     # R = high byte
rgba[...,1]=(V&255).astype(np.uint8)    # G = low byte
rgba[...,3]=255
Image.fromarray(rgba,'RGBA').save(os.path.join(T,"heightmap_denoised_RG.png"))
print("wrote heightmap_denoised_RG.png (R=hi,G=lo)")
