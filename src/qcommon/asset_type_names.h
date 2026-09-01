#ifndef QCOMMON_ASSET_TYPE_NAMES_H
#define QCOMMON_ASSET_TYPE_NAMES_H

/* Canonical tag-to-type mappings shared by engine and module boundaries.
 * Mac symbols preserve fileData_s, XAnimParts, XAnim_s, XAnimTree_s,
 * XModel_s, XModelInfo_s, and XModelSurfs_s.  Their public spellings follow
 * the original CoD/Quake convention: lower-case records use a _t name while
 * the XAnim/XModel records retain their family name and drop a trailing _s.
 * The XModel payload and XSurface records follow that same established
 * convention. */
typedef struct fileData_s fileData_t;
typedef struct XAnimParts XAnimParts;
typedef struct XAnim_s XAnim;
typedef struct XAnimTree_s XAnimTree;
typedef struct XModel_s XModel;
typedef struct XModelInfo_s XModelInfo;
typedef struct XModelSurfs_s XModelSurfs;
typedef struct XModelSurfsData_s XModelSurfsData;
typedef struct XModelPartsData_s XModelPartsData;
typedef struct XModelPartNameTable_s XModelPartNameTable;
typedef struct XModelPartNameTableSlot_s XModelPartNameTableSlot;
typedef struct XModelPartColl_s XModelPartColl;
typedef struct XModelLodInfo_s XModelLodInfo;
typedef struct XModelCollTriPlane_s XModelCollTriPlane;
typedef struct XModelCollTri_s XModelCollTri;
typedef struct XModelCollSurf_s XModelCollSurf;
typedef struct XModelConfigLod_s XModelConfigLod;
typedef struct XModelConfig XModelConfig;
typedef struct XStripInfo_s XStripInfo;
typedef struct XSimpleBlendInfo_s XSimpleBlendInfo;
typedef struct XSurfaceWeightedPoint_s XSurfaceWeightedPoint;
typedef struct XSurfaceRigidVert_s XSurfaceRigidVert;
typedef struct XSurfaceBlendVert_s XSurfaceBlendVert;
typedef struct XSurfaceBlendVertNoWeight_s XSurfaceBlendVertNoWeight;
typedef union XSurfaceVertexData_u XSurfaceVertexData;
typedef struct XSurface_s XSurface;
typedef struct XAnimToXModel XAnimToXModel;
typedef struct DObjTracePartRemap_s DObjTracePartRemap;
typedef struct DObjModel_s DObjModel;
typedef struct DObj_s DObj;

#endif
