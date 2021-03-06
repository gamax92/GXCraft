#include <cmath>

#include <grrlib.h>

#include "Fail3D.hpp"

static float focalLength;
static float vanishingPointX;
static float vanishingPointY;

static void rotateCoords(double *a, double *b, double angle) {
	double cosV = cos(angle);
	double sinV = sin(angle);
	double a1 = (*a * cosV) - (*b * sinV);
	double b1 = (*b * cosV) + (*a * sinV);
	*a = a1;
	*b = b1;
}

namespace Fail3D {
	void init(float fl) {
		focalLength = fl;
		vanishingPointX = rmode->fbWidth / 2.0f;
		vanishingPointY = rmode->efbHeight / 2.0f;
	}

	void translatePoint(guVector *polygon, guVector *center) {
		polygon->x -= center->x;
		polygon->y -= center->y;
		polygon->z -= center->z;
	}

	void rotatePoint(guVector *polygon, guVector *camera) {
		double x = polygon->x;
		double y = polygon->y;
		double z = polygon->z;

		rotateCoords(&x, &z, DegToRad(camera->y));
		rotateCoords(&y, &z, DegToRad(camera->x));

		polygon->x = x;
		polygon->y = y;
		polygon->z = z;
	}

	void calculatePointPosition(guVector *polygon) {
		float scale = fabs(focalLength / polygon->z);
		polygon->x = vanishingPointX + polygon->x * scale;
		polygon->y = vanishingPointY + polygon->y * scale;
	}
}
