# RB3 struct offsets (from Bank-5 DWARF band_r_wii.elf) — R1 probe constants

Source: `gdb -batch -ex 'ptype /o <T>'` on band_r_wii.elf (transcript: dwarf_offsets.txt).
Bank-5 layout; retail SZBE69 vtable ADDRESSES differ but MEMBER offsets match (same source/compiler).
Validate at runtime (G2) before trusting on retail.

## RndTransformable (the actual bone transform node; primary base @0)
- +0    vtable ptr
- +8    mParent : ObjOwnerPtr { +12 mOwner(Object*), +16 mPtr(RndTransformable*) }
- +20   mChildren (list, 8B)
- +32   mLocalXfm : Transform (64B) = Matrix3 rows @[32,48,64] (each Vector3 x,y,z at +0,+4,+8, 16B stride) + translation Vector3 @80
- +96   mWorldXfm : Transform (64B) = Matrix3 rows @[96,112,128] + translation Vector3 @144
- +260  mName (char*)  [in virtual base RndHighlightable::Object]

Matrix3 is 3 rows x Vector3(16B padded) = 48B; row r, col c float at +(32 + r*16 + c*4) for local, +(96 + r*16 + c*4) for world. Translation at +80 (local) / +144 (world).

## CharBone : public Object (size 92) — a bone DRIVER, not the transform
- +40 mPositionContext(int), +44 mScaleContext(int), +48 mRotation(Type), +52 mRotationContext(int)
- +56 mTarget : ObjPtr<CharBone> { +64 mPtr }
- +68 mWeights (list)
- +76 mTrans  : ObjPtr<RndTransformable> { +84 mPtr }  <-- follow to the posed Trans
- +88 mBakeOutAsTopLevel(bool)
NOTE: bone WORLD pose lives on the RndTransformable at CharBone+84 -> +96 (mWorldXfm), NOT in CharBone itself.
