#pragma once
#include "math/Mtx.h"
#include "obj/Data.h"
#include "rndobj/Draw.h"
#include "rndobj/Trans.h"
#include "rndobj/Mesh.h"

class RndLine : public RndDrawable, public RndTransformable {
public:
    // size 0x34
    class Point {
    public:
        Point() : v(0, 0, 0), c(-1) {}
        Point(const Point &p) {
            v = p.v;
            c = p.c;
            unk0 = p.unk0;
            unk1 = p.unk1;
            unk2 = p.unk2;
            unk3 = p.unk3;
            unk4 = p.unk4;
            unk5 = p.unk5;
            unk6 = p.unk6;
            unk7 = p.unk7;
            unk8 = p.unk8;
        }
        Vector3 v; // 0x0
        Hmx::Color32 c; // 0xc
        union {
            int unk[9]; // 0x10
            struct {
                float unk0;
                float unk1;
                float unk2;
                float unk3;
                float unk4;
                float unk5;
                float unk6;
                float unk7;
                float unk8;
            };
        };
    };

    class VertsMap {
    public:
        int t; // should be enum Type
        RndMesh::Vert *v;
    };

    RndLine();
    OBJ_CLASSNAME(Line);
    OBJ_SET_TYPE(Line);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void UpdateSphere();
    virtual float GetDistanceToPlane(const Plane &, Vector3 &);
    virtual bool MakeWorldSphere(Sphere &, bool);
    virtual void Mats(std::list<class RndMat *> &mats, bool) {
        if (mMat)
            mats.push_back(mMat);
    }
    virtual void DrawShowing();
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual int CollidePlane(const Plane &);
    virtual void Highlight() { RndDrawable::Highlight(); }
    virtual ~RndLine();
    virtual RndMesh *Mesh() const { return mMesh; }
    virtual void Print();

    void UpdateInternal();
    void SetNumPoints(int);
    void SetPointPos(int, const Vector3 &);
    void SetUpdate(bool);
    void SetMat(RndMat *);
    void UpdateLine(const Transform &, float);
    void UpdateLine(Point *, Point *);
    void UpdateLinePair(Point *, Point *);
    void MapVerts(int, VertsMap &);
    void SetPointColor(int, const Hmx::Color32 &, bool);
    void SetPointsColor(int, int, const Hmx::Color32 &);

    DataNode OnSetMat(const DataArray *);

    int NumPoints() const { return mPoints.size(); }
    Point &PointAt(int idx) { return mPoints[idx]; }
    float GetWidth() const { return mWidth; }
    void SetWidth(float w) { mWidth = w; }

    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(RndLine)
    static void Init() { REGISTER_OBJ_FACTORY(RndLine) }

    float mWidth; // 0xb0
    std::vector<Point> mPoints; // 0xb4
    RndMesh *mMesh; // 0xbc
    float mFoldAngle; // 0xc0
    float mFoldCos; // 0xc4
    ObjPtr<RndMat> mMat; // 0xc8
};

inline BinStream &operator>>(BinStream &bs, RndLine::Point &point) {
    bs >> point.v >> point.c;
    return bs;
}