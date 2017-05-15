#ifndef _Delta_Mush_CPU_Node
#define _Delta_Mush_CPU_Node

#include <maya/MPxDeformerNode.h>
#include <maya/MTypeId.h>
#include <maya/MIntArray.h>
#include <maya/MVectorArray.h>
#include <maya/MPointArray.h>
#include <maya/MGlobal.h>
#include <maya/MFnNumericAttribute.h>
#include <vector>

struct point_data
{
	MIntArray neighbours;
	MVectorArray delta;
	int size;
	double delta_len;
};

class Delta_Mush_CPU : public MPxDeformerNode
{
public:
						Delta_Mush_CPU();
	virtual				~Delta_Mush_CPU(); 
	static  void*		creator();
	static  MStatus		initialize();

	virtual MStatus deform(MDataBlock &data, MItGeometry &iter, const MMatrix &mat, unsigned int mIndex) override;
	virtual MStatus setDependentsDirty(const MPlug &plug, MPlugArray &plugArray) override;

private:
	void initData(MObject &mesh, int iters);
	void averageRelax(MPointArray &source, MPointArray &target, int iter, double amountV);
	void computeDelta(MPointArray &source, MPointArray &target);
	void rebindData(MObject &mesh, int iter, double amount);

	MPointArray targetPos;
	std::vector<point_data> dataPoints;
	bool initialized;

public:

	// There needs to be a MObject handle declared for each attribute that
	// the node will have.  These handles are needed for getting and setting
	// the values later.
	//
	static	MTypeId		id;
	static  MObject		referenceMush;		
	static  MObject		rebind;		
	static  MObject		iterations;
	static  MObject		useMulti;
	static  MObject		applyDelta;
	static  MObject		amount;
	static  MObject		mapMult;
	static  MObject		globalScale;

};

#endif
