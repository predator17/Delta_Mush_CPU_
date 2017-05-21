#include "Delta_Mush_CPU_Node.h"

#include <maya/MGlobal.h>
#include <maya/MPxDeformerNode.h>
#include <maya/MItGeometry.h>
#include <maya/MItMeshVertex.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MFnMesh.h>
#include <maya/MPointArray.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>

#define SMALL (float)1e-6

MTypeId     Delta_Mush_CPU::id( 0x0011ff84 );

MObject     Delta_Mush_CPU::rebind;
MObject     Delta_Mush_CPU::referenceMesh;
MObject     Delta_Mush_CPU::iterations;
MObject     Delta_Mush_CPU::applyDelta;
MObject     Delta_Mush_CPU::amount;
MObject     Delta_Mush_CPU::globalScale;

Delta_Mush_CPU::Delta_Mush_CPU(): initialized(false)
{
	targetPos.setLength(0);
}

void* Delta_Mush_CPU::creator()
//
//	Description:
//		this method exists to give Maya a way to create new objects
//      of this type. 
//
//	Return Value:
//		a new object of this type
//
{
	return new Delta_Mush_CPU();
}

MStatus Delta_Mush_CPU::initialize()
{
	MFnNumericAttribute numericAttr;
	MFnTypedAttribute tAttr;

	// global scale
	globalScale = numericAttr.create("globalScale", "gls", MFnNumericData::kDouble, 1.0);
	numericAttr.setKeyable(true);
	numericAttr.setStorable(true);
	numericAttr.setMin(0.0001);
	addAttribute(globalScale);

	rebind = numericAttr.create("rebind", "rbn", MFnNumericData::kBoolean, 0);
	numericAttr.setKeyable(true);
	numericAttr.setStorable(true);
	addAttribute(globalScale);

	applyDelta = numericAttr.create("applyDelta", "apdlt", MFnNumericData::kDouble, 1.0);
	numericAttr.setKeyable(true);
	numericAttr.setStorable(true);
	numericAttr.setMin(0);
	numericAttr.setMax(1);
	addAttribute(applyDelta);

	iterations = numericAttr.create("iterations", "itr", MFnNumericData::kInt, 0);
	numericAttr.setKeyable(true);
	numericAttr.setStorable(true);
	numericAttr.setMin(0);
	addAttribute(iterations);

	amount = numericAttr.create("amount", "am", MFnNumericData::kDouble, 0.5);
	numericAttr.setKeyable(true);
	numericAttr.setStorable(true);
	numericAttr.setMin(0);
	numericAttr.setMax(1);
	addAttribute(amount);

	referenceMesh = tAttr.create("referenceMesh", "rfm", MFnData::kMesh);
	tAttr.setKeyable(true);
	tAttr.setStorable(true);
	tAttr.setWritable(true);
	addAttribute(referenceMesh);

	attributeAffects(referenceMesh, outputGeom);
	attributeAffects(rebind, outputGeom);
	attributeAffects(iterations, outputGeom);
	attributeAffects(applyDelta, outputGeom);
	attributeAffects(amount, outputGeom);
	attributeAffects(globalScale, outputGeom);

	MGlobal::executeCommand("makePaintable -attrType multiFloat -sm deformer Delta_Mush_CPU weights");

	return MS::kSuccess;
}

MStatus Delta_Mush_CPU::deform(MDataBlock& data, MItGeometry& iter, const MMatrix& mat, unsigned int mIndex)
{
	// preliminary check: if the reference mesh is connected
	MPlug refMeshPlug(thisMObject(), referenceMesh);

	if (refMeshPlug.isConnected() == false)
	{
		std::cout << "reference mesh is not connected" << std::endl;
		return MS::kNotImplemented;
	}

	//getting needed data
	MObject referenceMeshV = data.inputValue(referenceMesh).asMesh();
	double envelopeV = data.inputValue(envelope).asFloat();
	int iterationsV = data.inputValue(iterations).asInt;
	double applyDeltaV = data.inputValue(applyDelta).asDouble;
	double amountV = data.inputValue(amount).asDouble;
	bool rebindV = data.inputValue(rebind).asBool;
	double globalScaleV = data.inputValue(globalScale).asDouble();

	//extracting the points
	MPointArray pos;
	iter.allPositions(pos, MSpace::kWorld);

	if (initialized == false || rebindV == true)
	{
		//binding the mesh
		rebindData(referenceMeshV, iterationsV, amountV);
		initialized = true;
	}

	if (envelopeV < SMALL)
	{ return MS::kSuccess; }

	int size = iter.exactCount();
	int i, n;
	MVector delta, v1, v2, cross;

	double weight;
	MMatrix mat;
	MPointArray final;

	// perform the smooth
	averageRelax(pos, targetPos, iterationsV, amountV);
	if (iterationsV == 0)
		return MS::kSuccess;
	else
		final.copy(targetPos);

	//looping the vertices 
}

MStatus Delta_Mush_CPU::setDependentsDirty(const MPlug & plug, MPlugArray & plugArray)
{
	return MStatus();
}

void Delta_Mush_CPU::initData(MObject & mesh)
{
	//building MFn mesh
	MFnMesh meshFn(mesh);
	//extract the number of vertices
	int size = meshFn.numVertices();

	//scaling the data points array
	dataPoints.resize(size);

	MPointArray pos, res;
	//using a mesh vertex iterator to extract the neighbours
	MItMeshVertex iter(mesh);
	iter.reset();
	// extracting the world position
	meshFn.getPoints(pos, MSpace::kWorld);

	MVectorArray arr;
	// loop the vertices
	for (int i = 0; i < size; ++i, iter.next())
	{
		// create a new point
		point_data pt;
		//query the neighbours
		iter.getConnectedVertices(pt.neighbours);
		// extract neighbour size
		pt.size = pt.neighbours.length();
		//set the point in the right array
		dataPoints[i] = pt;

		//pre allocating the deltas array
		arr = MVectorArray();
		arr.setLength(pt.size);
		dataPoints[i].delta = arr;
	}
}

void Delta_Mush_CPU::averageRelax(MPointArray & source, MPointArray & target, int iter, double amountV)
{
}

void Delta_Mush_CPU::computeDelta(MPointArray & source, MPointArray & target)
{
}

void Delta_Mush_CPU::rebindData(MObject & mesh, int iter, double amount)
{
	initData(mesh);
	MPointArray posRev, back;
	MFnMesh meshFn(mesh);
	meshFn.getPoints(posRev, MSpace::kObject);
	back.copy(posRev);
	averageRelax(posRev, back, iter, amount);
	computeDelta(posRev, back);
}

