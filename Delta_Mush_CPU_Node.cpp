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

