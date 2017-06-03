#include "Delta_Mush_CPU_Node.h"

#include <maya/MGlobal.h>
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

// arguments are type matters only, ignore the argument names, they don't have to be the same name as the header files.
MStatus Delta_Mush_CPU::deform(MDataBlock& data, MItGeometry& iter, const MMatrix& localTOWorldMatrix, unsigned int mIndex)
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
	int iterationsV = data.inputValue(iterations).asInt();
	double applyDeltaV = data.inputValue(applyDelta).asDouble();
	double amountV = data.inputValue(amount).asDouble();
	bool rebindV = data.inputValue(rebind).asBool();
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

	//looping the vertices and re-apply the delta
	for (i = 0; i < size; i++)
	{
		delta = MVector(0, 0, 0);
		if (applyDeltaV >= SMALL)
		{
			// loop the neighbours
			for (n = 0; n < dataPoints[i].size - 1; n++)
			{
				//extract the next two neighbours and compute the vector
				v1 = targetPos[dataPoints[i].neighbours[n]] - targetPos[i];
				v2 = targetPos[dataPoints[i].neighbours[n + 1]] - targetPos[i];

				v1.normalize();
				v2.normalize();

				cross = v1 ^ v2;
				v2 = cross ^ v1;

				mat = MMatrix();
				mat[0][0] = v1.x;
				mat[0][1] = v1.y;
				mat[0][2] = v1.z;
				mat[0][3] = 0;
				mat[1][0] = v2.x;
				mat[1][1] = v2.y;
				mat[1][2] = v2.z;
				mat[1][3] = 0;
				mat[2][0] = cross.x;
				mat[2][1] = cross.y;
				mat[2][2] = cross.z;
				mat[2][3] = 0;
				mat[3][0] = 0;
				mat[3][1] = 0;
				mat[3][2] = 0;
				mat[3][3] = 1;
				
				//accumulate the newly computed delta
				delta += ( dataPoints[i].delta[n] * mat);
			}
		}

		// averaging
		delta /= double(dataPoints[i].size);
		//scaling the vertex
		delta = delta.normal() * (dataPoints[i].deltaLen * applyDeltaV * globalScaleV);
		// adding the delta to the position
		final[i] += delta;

		// generate a  vector from the final position compute and the starting one and we scale it by the weights and the envelope
		delta = final[i] - pos[i];

		//querying the weight
		weight = weightValue(data, mIndex, i);
		// finally setting the final position
		final[i] = pos[i] + (delta * weight * envelopeV);
	}

	// set all the points
	iter.setAllPositions(final);

	return MStatus::kSuccess;

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
	// extracting the world position of the reference mesh
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
		arr = MVectorArray(); // bake down the topology map
		arr.setLength(pt.size);
		dataPoints[i].delta = arr;
	}
}

void Delta_Mush_CPU::averageRelax(MPointArray & source, MPointArray & target, int iter, double amountV)
{
	// rescale the target array if needed
	int size = source.length();
	target.setLength(size);

	// making a copy of the original
	MPointArray copy;
	copy.copy(source);

	MVector temp;
	int i, n, it;
	// looping for how many smooth iterations
	for (it = 0; it < iter; it++)
	{
		//looping for the mesh size
		for (i = 0; i < size; i++)
		{
			//reset the vector
			temp = MVector(0, 0, 0);
			// loop the neighbours
			for (n = 0; n < dataPoints[i].size; n++)
			{
				// read the neighbour position and accumulate 
				// dataPoints[i] this access the data of the vertex
				temp += copy[dataPoints[i].neighbours[n]];
			}

			// perform the average
			temp /= double(dataPoints[i].size);

			// scale the final position by the amount from user setting
			target[i] = copy[i] + (temp - copy[i])*amountV;

		}
		// copy the target array to be the source of next iteration
		copy.copy(target);
	}

}

void Delta_Mush_CPU::computeDelta(MPointArray & source, MPointArray & target)
{
	int size = source.length();
	MVectorArray arr;
	MVector delta, v1, v2, cross;
	int i, n;
	MMatrix mat;

	//build the matrix
	for (i = 0; i < size; i++)
	{
		delta = MVector(source[i] - target[i]);

		dataPoints[i].deltaLen = delta.length();

		//get tangent matrices
		for (n = 0; n < dataPoints[i].size - 1; n++)
		{
			v1 = target[dataPoints[i].neighbours[n]] - target[i];
			v2 = target[dataPoints[i].neighbours[n+1]] - target[i];

			v1.normalize();
			v2.normalize();

			cross = v1 ^ v2;
			v2 = cross ^ v1;

			mat = MMatrix();
			mat[0][0] = v1.x;
			mat[0][1] = v1.y;
			mat[0][2] = v1.z;
			mat[0][3] = 0;
			mat[1][0] = v2.x;
			mat[1][1] = v2.y;
			mat[1][2] = v2.z;
			mat[1][3] = 0;
			mat[2][0] = cross.x;
			mat[2][1] = cross.y;
			mat[2][2] = cross.z;
			mat[2][3] = 0;
			mat[3][0] = 0;
			mat[3][1] = 0;
			mat[3][2] = 0;
			mat[3][3] = 1;
			
			dataPoints[i].delta[n] = MVector( delta * mat.inverse());
		}
	}
}

void Delta_Mush_CPU::rebindData(MObject & mesh, int iter, double amount)
{
	// basically resized arrays and backing down neighbours
	initData(mesh);
	MPointArray posRev, back;
	MFnMesh meshFn(mesh);
	meshFn.getPoints(posRev, MSpace::kObject);
	back.copy(posRev);
	// smooth function
	averageRelax(posRev, back, iter, amount);
	// computing the delta
	computeDelta(posRev, back);
}

MStatus Delta_Mush_CPU::setDependentsDirty(const MPlug & plug, MPlugArray & plugArray)
{
	MStatus status;
	if (plug == iterations || plug == amount)
	{
		initialized = 0;
	}
	return MS::kSuccess;
}
