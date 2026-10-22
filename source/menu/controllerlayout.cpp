/****************************************************************************
 * Snes9x GX
 * Daryl Borth 2026
 * controllerlayout.cpp
 *
 * The layout tables (see controllerlayout.h).
 ***************************************************************************/

#include "controllerlayout.h"

const ControlSpot * ControllerLayout::find(int id) const
{
	if(id == 0)
		return nullptr;
	for(size_t i = 0; i < spots.size(); i++)
		if(spots[i].id == id)
			return &spots[i];
	return nullptr;
}

static GuiPath mk(const char * cmds, const float * pts, size_t count)
{
	GuiPath p;
	size_t k = 0;
	for(; *cmds; cmds++)
	{
		p.cmds.push_back((uint8_t)*cmds);
		const size_t n = *cmds == 'M' || *cmds == 'L' ? 2 : (*cmds == 'C' ? 6 : 0);
		for(size_t i = 0; i < n && k < count; i++)
			p.pts.push_back(pts[k++]);
	}
	return p;
}

static ControlSpot spot(int id, CTL_SHAPE shape, float x, float y, float w, float h, float radius, const char * label, const GuiPath & path, unsigned flags)
{
	return ControlSpot{ id, shape, x, y, w, h, radius, label, path, flags };
}

static const float kWiiUPro_1[] = { 345.85f, 48.24f, 345.55f, 40.22f, 335.76f, 37.26f, 335.76f, 37.26f, 331.31f, 33.4f, 320.92f, 28.35f, 320.92f, 28.35f, 319.14f, 26.43f, 318.84f, 21.68f, 318.84f, 21.68f, 295.84f, 21.68f, 295.84f, 28.65f, 295.84f, 28.65f, 294.07f, 28.65f, 266.76f, 40.22f, 239.46f, 51.8f, 199.99f, 51.2f, 199.99f, 51.2f, 199.99f, 51.2f, 160.52f, 51.8f, 133.22f, 40.22f, 105.92f, 28.65f, 104.14f, 28.65f, 104.14f, 28.65f, 104.14f, 21.68f, 81.13f, 21.68f, 81.13f, 21.68f, 80.83f, 26.42f, 79.05f, 28.35f, 79.05f, 28.35f, 68.67f, 33.4f, 64.22f, 37.26f, 64.22f, 37.26f, 54.42f, 40.22f, 54.12f, 48.24f, 54.12f, 48.24f, 13.62f, 93.64f, 4.72f, 230.15f, -4.19f, 366.65f, 74.75f, 273.47f, 91.96f, 258.63f, 109.18f, 243.8f, 126.98f, 225.99f, 199.98f, 225.99f, 272.98f, 225.99f, 290.79f, 243.8f, 308.0f, 258.63f, 325.21f, 273.47f, 404.15f, 366.65f, 395.25f, 230.15f, 386.35f, 93.64f, 345.85f, 48.24f, 345.85f, 48.24f };
static const float kWiiUPro_2[] = { 295.84f, 21.69f, 295.84f, 12.76f, 295.89f, 11.78f, 296.05f, 10.86f, 296.49f, 9.61f, 297.14f, 8.52f, 297.99f, 7.61f, 299.02f, 6.89f, 299.8f, 6.53f, 300.66f, 6.26f, 302.06f, 6.06f, 312.63f, 6.04f, 313.6f, 6.1f, 314.96f, 6.38f, 315.78f, 6.7f, 316.52f, 7.11f, 317.19f, 7.61f, 317.78f, 8.2f, 318.5f, 9.23f, 318.86f, 10.01f, 319.12f, 10.86f, 319.33f, 12.26f, 319.34f, 21.69f };
static const float kWiiUPro_3[] = { 325.86f, 17.08f, 344.86f, 28.49f, 345.56f, 29.02f, 346.14f, 29.66f, 346.56f, 30.39f, 346.84f, 31.18f, 346.97f, 32.01f, 346.93f, 32.85f, 346.73f, 33.68f, 346.35f, 34.48f, 345.82f, 35.19f, 345.4f, 35.59f, 344.7f, 36.07f, 344.2f, 36.3f, 343.39f, 36.53f, 342.56f, 36.6f, 341.99f, 36.56f, 341.43f, 36.44f, 340.89f, 36.25f, 340.36f, 35.98f, 321.36f, 24.56f, 320.65f, 24.03f, 320.08f, 23.39f, 319.65f, 22.67f, 319.38f, 21.88f, 319.25f, 21.05f, 319.29f, 20.21f, 319.49f, 19.37f, 319.87f, 18.57f, 320.21f, 18.09f, 320.6f, 17.66f, 321.27f, 17.13f, 322.02f, 16.75f, 322.83f, 16.53f, 323.66f, 16.45f, 324.51f, 16.55f, 325.33f, 16.81f };
static const float kWiiUPro_4[] = { 104.15f, 21.69f, 104.15f, 12.76f, 104.09f, 11.78f, 103.93f, 10.86f, 103.5f, 9.61f, 102.85f, 8.52f, 102.0f, 7.61f, 100.96f, 6.89f, 100.18f, 6.53f, 99.33f, 6.26f, 97.93f, 6.06f, 87.36f, 6.04f, 86.38f, 6.1f, 85.03f, 6.38f, 84.21f, 6.7f, 83.46f, 7.11f, 82.79f, 7.61f, 82.21f, 8.2f, 81.49f, 9.23f, 81.12f, 10.01f, 80.86f, 10.86f, 80.66f, 12.26f, 80.64f, 21.69f };
static const float kWiiUPro_5[] = { 74.12f, 17.08f, 55.13f, 28.49f, 54.42f, 29.02f, 53.85f, 29.66f, 53.42f, 30.39f, 53.14f, 31.18f, 53.02f, 32.01f, 53.05f, 32.85f, 53.26f, 33.68f, 53.63f, 34.48f, 54.16f, 35.19f, 54.58f, 35.59f, 55.28f, 36.07f, 55.79f, 36.3f, 56.59f, 36.53f, 57.43f, 36.6f, 57.99f, 36.56f, 58.55f, 36.44f, 59.1f, 36.25f, 59.62f, 35.98f, 78.62f, 24.56f, 79.33f, 24.03f, 79.9f, 23.39f, 80.33f, 22.67f, 80.61f, 21.88f, 80.73f, 21.05f, 80.7f, 20.21f, 80.49f, 19.37f, 80.11f, 18.57f, 79.78f, 18.09f, 79.38f, 17.66f, 78.71f, 17.13f, 77.96f, 16.75f, 77.16f, 16.53f, 76.32f, 16.45f, 75.48f, 16.55f, 74.65f, 16.81f };
static const float kWiiUPro_6[] = { 157.24f, 175.84f, 157.24f, 178.61f, 154.99f, 180.87f, 152.21f, 180.87f, 137.06f, 180.87f, 137.06f, 196.02f, 137.06f, 198.79f, 134.8f, 201.05f, 132.03f, 201.05f, 121.94f, 201.05f, 119.17f, 201.05f, 116.92f, 198.79f, 116.92f, 196.02f, 116.92f, 180.86f, 101.76f, 180.86f, 98.99f, 180.86f, 96.74f, 178.61f, 96.74f, 175.84f, 96.74f, 165.75f, 96.74f, 162.98f, 98.99f, 160.73f, 101.76f, 160.73f, 116.92f, 160.73f, 116.92f, 145.57f, 116.92f, 142.8f, 119.17f, 140.55f, 121.94f, 140.55f, 132.03f, 140.55f, 134.8f, 140.55f, 137.06f, 142.8f, 137.06f, 145.57f, 137.06f, 160.73f, 152.22f, 160.73f, 154.99f, 160.73f, 157.24f, 162.98f, 157.24f, 165.75f, 157.24f, 175.84f, 157.24f, 175.84f };
static const float kWiiUPro_7[] = { 116.92f, 160.73f, 116.93f, 145.22f, 117.06f, 144.39f, 117.38f, 143.47f, 117.78f, 142.76f, 118.39f, 142.02f, 119.13f, 141.4f, 119.99f, 140.94f, 120.93f, 140.65f, 121.94f, 140.55f, 132.54f, 140.57f, 133.37f, 140.73f, 134.13f, 141.01f, 134.84f, 141.4f, 135.58f, 142.02f, 136.2f, 142.76f, 136.6f, 143.47f, 136.92f, 144.39f, 137.05f, 145.22f, 137.06f, 160.73f, 126.99f, 170.8f };
static const float kWiiUPro_8[] = { 137.06f, 180.87f, 137.04f, 196.37f, 136.92f, 197.2f, 136.59f, 198.13f, 136.2f, 198.83f, 135.58f, 199.57f, 134.84f, 200.19f, 133.98f, 200.65f, 133.04f, 200.94f, 132.03f, 201.05f, 121.43f, 201.02f, 120.61f, 200.87f, 119.84f, 200.59f, 119.13f, 200.19f, 118.39f, 199.57f, 117.78f, 198.83f, 117.38f, 198.13f, 117.06f, 197.2f, 116.93f, 196.37f, 116.92f, 180.87f, 126.99f, 170.8f };
static const float kWiiUPro_9[] = { 116.92f, 180.87f, 101.42f, 180.85f, 100.59f, 180.72f, 99.66f, 180.4f, 98.95f, 180.0f, 98.21f, 179.39f, 97.6f, 178.65f, 97.2f, 177.94f, 96.92f, 177.17f, 96.76f, 176.35f, 96.74f, 165.75f, 96.84f, 164.74f, 97.13f, 163.8f, 97.6f, 162.94f, 98.21f, 162.2f, 98.95f, 161.58f, 99.66f, 161.19f, 100.59f, 160.86f, 101.42f, 160.74f, 116.92f, 160.73f, 126.99f, 170.8f };
static const float kWiiUPro_10[] = { 157.24f, 175.84f, 157.17f, 176.69f, 156.96f, 177.49f, 156.63f, 178.24f, 156.19f, 178.91f, 155.53f, 179.61f, 154.89f, 180.1f, 154.17f, 180.47f, 153.39f, 180.73f, 152.73f, 180.84f, 152.21f, 180.87f, 137.06f, 180.87f, 126.99f, 170.8f, 137.06f, 160.73f, 152.22f, 160.73f, 153.06f, 160.8f, 153.71f, 160.95f, 154.32f, 161.19f, 154.89f, 161.5f, 155.53f, 161.98f, 156.09f, 162.56f, 156.47f, 163.08f, 156.84f, 163.8f, 157.06f, 164.41f, 157.19f, 165.07f, 157.24f, 165.75f };
static ControllerLayout buildWiiUPro()
{
	ControllerLayout l;
	l.width = 400.0f;	l.height = 307.59f;
	l.bodyPaths.push_back(mk("MCCCLLCCCCLLCCCCCCCCCZ", kWiiUPro_1, 106));
	l.spots.push_back(spot(INPUT_TRIGGER_ZR, CTL_SHAPE::PATH, 295.84f, 6.04f, 23.51f, 15.65f, 0.0f, "ZR", mk("MLLLLLLLLLLLLLLLLLLLLLLZ", kWiiUPro_2, 46), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_R, CTL_SHAPE::PATH, 319.25f, 16.45f, 27.72f, 20.15f, 0.0f, "R", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kWiiUPro_3, 74), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZL, CTL_SHAPE::PATH, 80.64f, 6.04f, 23.51f, 15.65f, 0.0f, "ZL", mk("MLLLLLLLLLLLLLLLLLLLLLLZ", kWiiUPro_4, 46), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_L, CTL_SHAPE::PATH, 53.02f, 16.45f, 27.72f, 20.15f, 0.0f, "L", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kWiiUPro_5, 74), 0u));
	l.spots.push_back(spot(INPUT_THUMB_L, CTL_SHAPE::STICK, 58.76f, 84.76f, 49.21f, 49.21f, 30.89f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_THUMB_R, CTL_SHAPE::STICK, 292.31f, 84.76f, 49.21f, 49.21f, 30.89f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 96.74f, 140.55f, 60.5f, 60.5f, 0.0f, nullptr, mk("MCLLCLCLLCLCLLCLCLLCLLZ", kWiiUPro_6, 76), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 116.92f, 140.55f, 20.14f, 30.25f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLLLLZ", kWiiUPro_7, 42), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 116.92f, 170.8f, 20.14f, 30.25f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLLLLZ", kWiiUPro_8, 42), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 96.74f, 160.73f, 30.25f, 20.14f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLLLLZ", kWiiUPro_9, 42), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 126.99f, 160.73f, 30.25f, 20.14f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLLLLLLLLLZ", kWiiUPro_10, 52), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_X, CTL_SHAPE::CIRCLE, 259.96f, 133.13f, 27.25f, 27.25f, 0.0f, "X", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_Y, CTL_SHAPE::CIRCLE, 234.89f, 158.21f, 27.25f, 27.25f, 0.0f, "Y", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 285.04f, 158.21f, 27.25f, 27.25f, 0.0f, "A", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::CIRCLE, 259.96f, 183.28f, 27.25f, 27.25f, 0.0f, "B", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_MINUS, CTL_SHAPE::CIRCLE, 157.13f, 113.84f, 20.13f, 20.13f, 0.0f, "-", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_HOME, CTL_SHAPE::CIRCLE, 188.74f, 112.65f, 22.51f, 22.51f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::CIRCLE, 222.72f, 113.84f, 20.13f, 20.13f, 0.0f, "+", GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::CIRCLE, 188.74f, 159.54f, 22.51f, 22.51f, 0.0f, nullptr, GuiPath(), 0u));
	return l;
}

static const float kGamePad_1[] = { 69.42f, 22.35f, 50.88f, 22.88f, 40.38f, 25.62f, 31.85f, 32.19f, 17.62f, 43.12f, 10.54f, 61.35f, 9.31f, 90.19f, 8.81f, 101.54f, 9.04f, 158.0f, 9.58f, 163.65f, 12.0f, 188.23f, 18.5f, 203.62f, 30.46f, 213.27f, 37.42f, 218.85f, 45.54f, 221.65f, 59.04f, 223.12f, 64.96f, 223.77f, 326.58f, 224.12f, 336.92f, 223.46f, 352.15f, 222.58f, 361.12f, 220.23f, 368.69f, 215.19f, 378.27f, 208.81f, 385.23f, 198.38f, 389.19f, 184.62f, 391.19f, 177.58f, 392.27f, 171.04f, 393.08f, 161.35f, 393.69f, 154.35f, 393.69f, 89.88f, 393.12f, 83.27f, 391.54f, 65.27f, 387.73f, 52.35f, 381.04f, 42.31f, 378.23f, 38.08f, 371.69f, 31.77f, 367.62f, 29.38f, 361.31f, 25.69f, 352.81f, 23.5f, 341.35f, 22.73f, 334.85f, 22.27f, 84.85f, 21.96f, 69.42f, 22.35f };
static const float kGamePad_2[] = { 89.6f, 21.87f, 90.08f, 20.65f, 90.41f, 19.38f, 90.56f, 18.08f, 90.54f, 16.77f, 90.35f, 15.48f, 90.0f, 14.22f, 89.48f, 13.02f, 88.82f, 11.9f, 88.0f, 10.87f, 87.07f, 9.95f, 86.02f, 9.17f, 84.88f, 8.53f, 83.67f, 8.05f, 82.4f, 7.73f, 81.1f, 7.57f, 79.79f, 7.59f, 78.5f, 7.78f, 26.58f, 18.81f, 25.32f, 19.17f, 24.71f, 19.41f, 23.54f, 20.0f, 22.47f, 20.74f, 21.49f, 21.62f, 21.05f, 22.1f, 20.27f, 23.15f, 19.63f, 24.29f, 19.15f, 25.5f, 18.97f, 26.13f, 18.82f, 26.77f, 18.67f, 28.07f, 18.69f, 29.38f, 18.76f, 30.03f, 19.03f, 31.31f, 19.47f, 32.54f, 20.06f, 33.71f, 20.8f, 34.78f, 21.68f, 35.76f, 22.67f, 36.61f, 23.77f, 37.32f, 24.35f, 37.62f, 25.5f, 38.08f, 26.53f, 36.96f, 27.79f, 35.69f, 29.1f, 34.48f, 30.45f, 33.31f, 32.71f, 31.55f, 34.03f, 30.63f, 35.38f, 29.76f, 36.78f, 28.95f, 38.22f, 28.2f, 39.72f, 27.49f, 41.27f, 26.83f, 42.87f, 26.23f, 44.55f, 25.67f, 47.49f, 24.85f, 50.65f, 24.15f, 54.04f, 23.57f, 58.44f, 23.02f, 62.42f, 22.68f, 66.7f, 22.44f, 70.39f, 22.33f, 89.42f, 22.22f };
static const float kGamePad_3[] = { 376.56f, 36.61f, 377.55f, 35.76f, 378.43f, 34.78f, 379.17f, 33.71f, 379.76f, 32.54f, 380.2f, 31.31f, 380.47f, 30.03f, 380.54f, 29.38f, 380.56f, 28.07f, 380.41f, 26.77f, 380.08f, 25.5f, 379.6f, 24.29f, 378.96f, 23.15f, 378.18f, 22.1f, 377.27f, 21.16f, 376.24f, 20.35f, 375.12f, 19.68f, 373.91f, 19.17f, 372.65f, 18.81f, 320.73f, 7.78f, 319.44f, 7.59f, 318.13f, 7.57f, 317.48f, 7.63f, 316.2f, 7.87f, 315.57f, 8.05f, 314.35f, 8.53f, 313.21f, 9.17f, 312.16f, 9.95f, 311.23f, 10.87f, 310.8f, 11.37f, 310.06f, 12.45f, 309.47f, 13.61f, 309.23f, 14.22f, 309.03f, 14.85f, 308.76f, 16.12f, 308.69f, 16.77f, 308.67f, 18.08f, 308.82f, 19.38f, 309.15f, 20.65f, 309.63f, 21.87f, 309.95f, 22.47f, 332.57f, 22.6f, 341.35f, 22.73f, 345.24f, 23.06f, 348.39f, 23.43f, 351.37f, 23.89f, 354.18f, 24.44f, 357.7f, 25.3f, 360.55f, 26.19f, 362.09f, 26.75f, 363.92f, 27.5f, 366.65f, 28.84f, 368.46f, 29.91f, 370.55f, 31.43f, 372.28f, 32.85f, 373.55f, 33.97f, 374.8f, 35.15f, 376.38f, 36.73f };
static const float kGamePad_4[] = { 145.99f, 21.65f, 146.08f, 21.1f, 146.15f, 20.0f, 146.08f, 18.9f, 145.99f, 18.35f, 145.87f, 17.81f, 145.51f, 16.76f, 145.02f, 15.77f, 144.4f, 14.85f, 144.05f, 14.42f, 143.27f, 13.64f, 142.39f, 12.97f, 141.43f, 12.41f, 140.93f, 12.18f, 139.88f, 11.83f, 139.34f, 11.7f, 138.25f, 11.56f, 108.46f, 11.54f, 107.36f, 11.61f, 106.81f, 11.7f, 106.27f, 11.83f, 105.22f, 12.18f, 104.23f, 12.67f, 103.76f, 12.97f, 102.88f, 13.64f, 102.1f, 14.42f, 101.43f, 15.3f, 101.13f, 15.77f, 100.64f, 16.76f, 100.29f, 17.81f, 100.16f, 18.35f, 100.02f, 19.45f, 100.0f, 20.0f, 100.07f, 21.1f, 100.29f, 22.2f, 145.87f, 22.17f };
static const float kGamePad_5[] = { 298.94f, 22.19f, 299.16f, 21.1f, 299.23f, 20.0f, 299.21f, 19.45f, 299.07f, 18.35f, 298.94f, 17.81f, 298.59f, 16.76f, 298.1f, 15.77f, 297.8f, 15.3f, 297.13f, 14.42f, 296.75f, 14.02f, 295.92f, 13.29f, 295.0f, 12.67f, 294.01f, 12.18f, 292.96f, 11.83f, 292.42f, 11.7f, 291.32f, 11.56f, 261.54f, 11.54f, 260.98f, 11.56f, 259.89f, 11.7f, 259.35f, 11.83f, 258.3f, 12.18f, 257.31f, 12.67f, 256.84f, 12.97f, 255.96f, 13.64f, 255.18f, 14.42f, 254.5f, 15.3f, 254.21f, 15.77f, 253.72f, 16.76f, 253.37f, 17.81f, 253.24f, 18.35f, 253.1f, 19.45f, 253.08f, 20.0f, 253.15f, 21.1f, 253.37f, 22.19f, 253.4f, 22.3f, 253.46f, 22.3f, 298.87f, 22.42f };
static const float kGamePad_6[] = { 96.92f, 123.08f, 96.92f, 182.31f, 306.15f, 182.31f, 306.15f, 63.85f, 96.92f, 63.85f };
static const float kGamePad_7[] = { 63.92f, 101.81f, 64.04f, 108.27f, 70.5f, 108.38f, 76.92f, 108.46f, 76.92f, 113.08f, 76.92f, 117.69f, 70.38f, 117.69f, 63.85f, 117.69f, 63.85f, 124.23f, 63.85f, 130.77f, 59.23f, 130.77f, 54.62f, 130.77f, 54.62f, 124.23f, 54.62f, 117.69f, 48.08f, 117.69f, 41.54f, 117.69f, 41.54f, 113.08f, 41.54f, 108.46f, 48.0f, 108.38f, 54.42f, 108.27f, 54.54f, 101.81f, 54.62f, 95.38f, 59.23f, 95.38f, 63.85f, 95.38f, 63.92f, 101.81f };
static const float kGamePad_8[] = { 63.92f, 101.81f, 64.04f, 108.27f, 59.23f, 113.08f, 54.42f, 108.27f, 54.62f, 95.38f, 63.85f, 95.38f };
static const float kGamePad_9[] = { 63.85f, 117.69f, 63.85f, 130.77f, 54.62f, 130.77f, 54.62f, 117.69f, 59.23f, 113.08f };
static const float kGamePad_10[] = { 59.23f, 113.08f, 54.62f, 117.69f, 41.54f, 117.69f, 41.54f, 108.46f, 54.42f, 108.27f };
static const float kGamePad_11[] = { 64.04f, 108.27f, 76.92f, 108.46f, 76.92f, 117.69f, 63.85f, 117.69f, 59.23f, 113.08f };
static ControllerLayout buildGamePad()
{
	ControllerLayout l;
	l.width = 400.0f;	l.height = 230.77f;
	l.bodyPaths.push_back(mk("MCCCCCCCCCCCCCCZ", kGamePad_1, 86));
	l.spots.push_back(spot(INPUT_TRIGGER_L, CTL_SHAPE::PATH, 18.67f, 7.57f, 71.89f, 30.51f, 0.0f, "L", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kGamePad_2, 126), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_R, CTL_SHAPE::PATH, 308.67f, 7.57f, 71.89f, 29.16f, 0.0f, "R", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kGamePad_3, 116), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZL, CTL_SHAPE::PATH, 100.0f, 11.54f, 46.15f, 10.66f, 0.0f, "ZL", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kGamePad_4, 72), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZR, CTL_SHAPE::PATH, 253.08f, 11.54f, 46.15f, 10.88f, 0.0f, "ZR", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kGamePad_5, 76), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 96.92f, 63.85f, 209.23f, 118.46f, 0.0f, nullptr, mk("MLLLLZ", kGamePad_6, 10), 0u));
	l.spots.push_back(spot(INPUT_THUMB_L, CTL_SHAPE::STICK, 27.75f, 53.12f, 30.92f, 30.92f, 21.54f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_THUMB_R, CTL_SHAPE::STICK, 343.6f, 53.2f, 30.92f, 30.92f, 21.54f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 41.54f, 95.38f, 35.38f, 35.38f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLLLLLLLLZ", kGamePad_7, 50), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 54.42f, 95.38f, 9.62f, 17.69f, 0.0f, nullptr, mk("MLLLLLZ", kGamePad_8, 12), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 54.62f, 113.08f, 9.23f, 17.69f, 0.0f, nullptr, mk("MLLLLZ", kGamePad_9, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 41.54f, 108.27f, 17.69f, 9.42f, 0.0f, nullptr, mk("MLLLLZ", kGamePad_10, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 59.23f, 108.27f, 17.69f, 9.42f, 0.0f, nullptr, mk("MLLLLZ", kGamePad_11, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_X, CTL_SHAPE::CIRCLE, 338.76f, 92.5f, 12.43f, 12.43f, 0.0f, "X", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_Y, CTL_SHAPE::CIRCLE, 323.17f, 108.02f, 12.27f, 12.27f, 0.0f, "Y", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 354.35f, 108.02f, 12.37f, 12.37f, 0.0f, "A", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::CIRCLE, 338.83f, 123.72f, 12.35f, 12.35f, 0.0f, "B", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::CIRCLE, 323.3f, 167.74f, 9.8f, 9.8f, 0.0f, "+", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_MINUS, CTL_SHAPE::CIRCLE, 323.36f, 148.06f, 9.7f, 9.7f, 0.0f, "-", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_HOME, CTL_SHAPE::CIRCLE, 193.08f, 200.0f, 13.85f, 13.85f, 0.0f, nullptr, GuiPath(), 0u));
	return l;
}

static const float kGameCube_1[] = { 361.37f, 71.2f, 361.37f, 71.2f, 355.19f, 39.14f, 309.62f, 48.41f, 309.62f, 48.41f, 283.37f, 34.1f, 213.94f, 31.73f, 197.57f, 31.45f, 119.41f, 32.52f, 90.23f, 48.41f, 90.23f, 48.41f, 44.66f, 39.14f, 38.48f, 71.2f, 38.48f, 71.2f, -6.71f, 101.33f, 6.43f, 156.95f, 6.43f, 156.95f, 1.79f, 213.34f, 0.05f, 317.17f, 37.71f, 317.17f, 75.37f, 317.17f, 75.17f, 219.9f, 75.17f, 219.9f, 75.17f, 219.9f, 81.74f, 230.71f, 86.76f, 245.0f, 91.78f, 259.29f, 129.25f, 298.3f, 169.8f, 255.43f, 210.35f, 212.56f, 154.35f, 169.68f, 154.35f, 169.68f, 154.35f, 162.87f, 167.87f, 159.14f, 199.92f, 158.48f, 199.92f, 158.48f, 199.92f, 158.48f, 231.98f, 159.14f, 245.5f, 162.87f, 245.5f, 169.68f, 245.5f, 169.68f, 189.49f, 212.56f, 230.05f, 255.43f, 270.6f, 298.3f, 308.07f, 259.29f, 313.09f, 245.0f, 318.11f, 230.71f, 324.68f, 219.9f, 324.68f, 219.9f, 324.68f, 219.9f, 324.49f, 317.17f, 362.14f, 317.17f, 399.8f, 317.17f, 398.06f, 213.33f, 393.43f, 156.94f, 393.43f, 156.94f, 406.56f, 101.33f, 361.37f, 71.2f };
static const float kGameCube_2[] = { 89.95f, 47.42f, 90.29f, 46.47f, 90.56f, 45.49f, 90.76f, 44.51f, 90.9f, 43.51f, 90.96f, 42.51f, 90.97f, 41.5f, 90.92f, 40.49f, 90.79f, 39.5f, 90.6f, 38.51f, 90.34f, 37.53f, 90.03f, 36.58f, 89.65f, 35.65f, 89.21f, 34.74f, 88.71f, 33.87f, 88.15f, 33.02f, 87.55f, 32.22f, 86.89f, 31.46f, 86.18f, 30.74f, 85.43f, 30.07f, 84.64f, 29.46f, 83.8f, 28.89f, 82.94f, 28.38f, 81.1f, 27.54f, 80.15f, 27.21f, 79.18f, 26.94f, 78.2f, 26.74f, 77.2f, 26.6f, 76.19f, 26.53f, 75.19f, 26.52f, 74.19f, 26.58f, 73.18f, 26.71f, 72.2f, 26.9f, 71.22f, 27.15f, 69.34f, 27.85f, 41.22f, 40.36f, 39.44f, 41.3f, 38.6f, 41.85f, 37.8f, 42.46f, 37.04f, 43.12f, 36.32f, 43.83f, 35.66f, 44.58f, 35.04f, 45.38f, 34.47f, 46.21f, 33.97f, 47.08f, 33.51f, 47.98f, 32.79f, 49.85f, 32.52f, 50.83f, 32.32f, 51.81f, 32.18f, 52.81f, 32.11f, 53.81f, 32.11f, 54.82f, 32.16f, 55.83f, 32.29f, 56.82f, 32.48f, 57.81f, 32.74f, 58.79f, 33.05f, 59.75f, 33.87f, 61.58f, 34.37f, 62.45f, 34.92f, 63.3f, 35.53f, 64.1f, 36.19f, 64.86f, 36.9f, 65.58f, 37.65f, 66.25f, 38.44f, 66.86f, 39.61f, 67.63f, 40.43f, 65.75f, 41.05f, 64.53f, 41.77f, 63.23f, 42.62f, 61.89f, 43.57f, 60.51f, 44.67f, 59.11f, 46.56f, 57.02f, 48.01f, 55.65f, 48.8f, 54.98f, 49.62f, 54.31f, 51.38f, 53.04f, 52.33f, 52.43f, 54.35f, 51.26f, 56.56f, 50.2f, 57.74f, 49.71f, 60.23f, 48.82f, 61.55f, 48.42f, 64.35f, 47.74f, 67.35f, 47.22f, 70.58f, 46.88f, 72.27f, 46.78f, 75.84f, 46.74f, 79.65f, 46.91f, 83.69f, 47.32f, 85.81f, 47.62f, 89.59f, 48.29f };
static const float kGameCube_3[] = { 360.72f, 67.43f, 361.56f, 66.86f, 362.35f, 66.25f, 363.1f, 65.58f, 363.81f, 64.86f, 364.47f, 64.1f, 365.08f, 63.3f, 365.63f, 62.45f, 366.13f, 61.58f, 366.95f, 59.75f, 367.26f, 58.79f, 367.52f, 57.81f, 367.71f, 56.82f, 367.84f, 55.83f, 367.89f, 54.82f, 367.89f, 53.81f, 367.82f, 52.81f, 367.68f, 51.81f, 367.48f, 50.83f, 367.21f, 49.85f, 366.49f, 47.98f, 366.03f, 47.08f, 365.53f, 46.21f, 364.96f, 45.38f, 364.34f, 44.58f, 363.68f, 43.83f, 362.96f, 43.12f, 362.2f, 42.46f, 361.4f, 41.85f, 360.56f, 41.3f, 358.78f, 40.36f, 330.66f, 27.85f, 328.78f, 27.15f, 327.8f, 26.9f, 326.82f, 26.71f, 325.81f, 26.58f, 324.81f, 26.52f, 323.81f, 26.53f, 322.8f, 26.6f, 321.8f, 26.74f, 320.82f, 26.94f, 319.85f, 27.21f, 318.9f, 27.54f, 317.06f, 28.38f, 316.2f, 28.89f, 315.36f, 29.46f, 314.57f, 30.07f, 313.82f, 30.74f, 313.11f, 31.46f, 312.45f, 32.22f, 311.85f, 33.02f, 311.29f, 33.87f, 310.79f, 34.74f, 309.97f, 36.58f, 309.66f, 37.53f, 309.4f, 38.51f, 309.21f, 39.5f, 309.08f, 40.49f, 309.03f, 41.5f, 309.04f, 42.51f, 309.1f, 43.51f, 309.24f, 44.51f, 309.44f, 45.49f, 309.71f, 46.47f, 310.4f, 48.26f, 314.05f, 47.62f, 316.16f, 47.32f, 318.21f, 47.08f, 320.21f, 46.91f, 324.01f, 46.74f, 327.58f, 46.78f, 330.92f, 47.03f, 334.03f, 47.46f, 335.51f, 47.74f, 338.3f, 48.42f, 339.63f, 48.82f, 342.11f, 49.71f, 343.29f, 50.2f, 345.5f, 51.26f, 347.53f, 52.43f, 349.38f, 53.67f, 350.24f, 54.31f, 351.84f, 55.65f, 352.59f, 56.33f, 353.96f, 57.71f, 355.19f, 59.11f, 356.28f, 60.51f, 357.24f, 61.89f, 358.08f, 63.23f, 358.81f, 64.53f, 359.42f, 65.75f, 360.27f, 67.7f };
static const float kGameCube_4[] = { 298.35f, 43.8f, 299.69f, 42.82f, 300.9f, 41.68f, 301.95f, 40.38f, 302.4f, 39.69f, 303.18f, 38.22f, 303.75f, 36.66f, 304.12f, 35.04f, 304.27f, 33.38f, 304.21f, 31.72f, 303.92f, 30.08f, 303.43f, 28.49f, 302.74f, 26.98f, 302.31f, 26.27f, 301.34f, 24.92f, 300.19f, 23.72f, 299.57f, 23.17f, 298.9f, 22.67f, 297.48f, 21.8f, 296.74f, 21.44f, 295.97f, 21.12f, 294.37f, 20.66f, 281.66f, 18.62f, 280.84f, 18.51f, 279.17f, 18.47f, 277.52f, 18.64f, 276.71f, 18.81f, 275.12f, 19.3f, 273.61f, 19.99f, 272.89f, 20.41f, 271.55f, 21.39f, 270.34f, 22.54f, 269.29f, 23.83f, 268.84f, 24.52f, 268.43f, 25.25f, 268.06f, 25.99f, 267.75f, 26.77f, 267.48f, 27.56f, 267.28f, 28.36f, 267.12f, 29.18f, 267.02f, 30.0f, 266.98f, 31.67f, 267.03f, 32.49f, 267.15f, 33.32f, 267.54f, 34.94f, 267.81f, 35.72f, 268.33f, 36.89f, 275.71f, 38.24f, 279.46f, 39.01f, 282.98f, 39.79f, 286.26f, 40.57f, 292.13f, 42.11f, 294.72f, 42.86f, 298.14f, 43.92f };
static const float kGameCube_5[] = { 272.31f, 193.68f, 250.5f, 199.06f, 238.51f, 217.68f, 243.92f, 240.86f, 263.98f, 252.62f, 286.51f, 246.89f, 298.14f, 226.11f, 291.78f, 204.9f, 272.31f, 193.68f };
static const float kGameCube_6[] = { 382.03f, 133.72f, 380.44f, 126.98f, 379.44f, 123.0f, 378.36f, 119.12f, 376.86f, 114.06f, 375.19f, 108.56f, 374.94f, 107.9f, 374.32f, 106.67f, 373.55f, 105.55f, 373.11f, 105.03f, 372.15f, 104.09f, 371.63f, 103.67f, 370.51f, 102.92f, 369.31f, 102.32f, 368.69f, 102.08f, 367.39f, 101.71f, 366.73f, 101.6f, 365.38f, 101.49f, 364.7f, 101.5f, 363.33f, 101.68f, 362.64f, 101.84f, 361.78f, 102.11f, 360.8f, 102.53f, 359.89f, 103.03f, 359.04f, 103.62f, 358.0f, 104.54f, 357.1f, 105.59f, 356.34f, 106.74f, 355.7f, 108.06f, 355.3f, 109.3f, 355.05f, 110.58f, 354.96f, 111.9f, 355.05f, 113.24f, 355.23f, 114.25f, 355.41f, 114.82f, 357.88f, 119.43f, 358.86f, 121.39f, 359.96f, 123.94f, 360.72f, 126.18f, 361.29f, 128.57f, 361.52f, 129.92f, 361.79f, 131.98f, 362.1f, 135.87f, 362.2f, 140.09f, 362.46f, 140.87f, 362.8f, 141.61f, 363.19f, 142.31f, 363.75f, 143.14f, 364.39f, 143.91f, 365.05f, 144.59f, 365.74f, 145.18f, 366.48f, 145.71f, 367.26f, 146.18f, 368.08f, 146.58f, 369.03f, 146.95f, 369.83f, 147.18f, 370.92f, 147.39f, 371.76f, 147.47f, 372.6f, 147.49f, 373.45f, 147.43f, 374.86f, 147.16f, 375.53f, 146.96f, 376.79f, 146.43f, 377.95f, 145.74f, 378.49f, 145.35f, 379.47f, 144.46f, 379.92f, 143.97f, 380.72f, 142.92f, 381.37f, 141.77f, 381.65f, 141.16f, 382.08f, 139.89f, 382.24f, 139.23f, 382.44f, 137.88f, 382.48f, 137.19f, 382.42f, 135.78f, 382.32f, 135.07f };
static const float kGameCube_7[] = { 291.14f, 101.57f, 292.01f, 102.41f, 292.97f, 103.15f, 294.16f, 103.85f, 295.29f, 104.35f, 296.42f, 104.7f, 297.59f, 104.93f, 298.78f, 105.03f, 299.99f, 104.98f, 300.89f, 104.84f, 301.66f, 104.64f, 306.35f, 102.13f, 308.73f, 100.96f, 311.3f, 99.88f, 313.55f, 99.16f, 315.92f, 98.64f, 318.41f, 98.27f, 320.27f, 98.08f, 322.62f, 97.92f, 324.68f, 97.84f, 327.01f, 97.82f, 327.95f, 97.49f, 328.85f, 97.07f, 329.7f, 96.58f, 330.74f, 95.81f, 331.46f, 95.14f, 332.12f, 94.4f, 332.7f, 93.6f, 333.06f, 93.02f, 333.61f, 91.87f, 333.96f, 90.9f, 334.21f, 89.91f, 334.37f, 88.9f, 334.42f, 87.96f, 334.39f, 86.86f, 334.26f, 85.92f, 334.08f, 85.15f, 333.72f, 84.04f, 333.24f, 83.0f, 332.96f, 82.51f, 332.33f, 81.58f, 331.97f, 81.15f, 331.2f, 80.34f, 330.79f, 79.98f, 329.89f, 79.31f, 328.93f, 78.74f, 328.42f, 78.49f, 327.36f, 78.08f, 326.81f, 77.92f, 325.67f, 77.67f, 325.08f, 77.59f, 323.55f, 77.54f, 322.46f, 77.62f, 321.38f, 77.81f, 313.96f, 79.56f, 309.97f, 80.56f, 303.3f, 82.46f, 295.46f, 84.82f, 294.8f, 85.07f, 294.17f, 85.36f, 292.98f, 86.06f, 291.91f, 86.89f, 290.96f, 87.85f, 290.15f, 88.91f, 289.49f, 90.07f, 288.97f, 91.3f, 288.62f, 92.59f, 288.43f, 93.93f, 288.42f, 95.29f, 288.6f, 96.68f, 288.82f, 97.57f, 289.16f, 98.54f, 289.71f, 99.64f, 290.37f, 100.64f };
static const float kGameCube_8[] = { 139.17f, 195.57f, 139.17f, 194.57f, 138.36f, 193.75f, 137.36f, 193.75f, 123.46f, 193.75f, 122.45f, 193.75f, 121.64f, 194.57f, 121.64f, 195.57f, 121.64f, 214.22f, 102.99f, 214.22f, 101.99f, 214.22f, 101.18f, 215.03f, 101.18f, 216.04f, 101.18f, 229.94f, 101.18f, 230.94f, 101.99f, 231.75f, 102.99f, 231.75f, 121.64f, 231.75f, 121.64f, 250.41f, 121.64f, 251.41f, 122.46f, 252.22f, 123.46f, 252.22f, 137.36f, 252.22f, 138.36f, 252.22f, 139.17f, 251.41f, 139.17f, 250.41f, 139.17f, 231.76f, 157.83f, 231.76f, 158.83f, 231.76f, 159.65f, 230.94f, 159.65f, 229.94f, 159.65f, 216.04f, 159.65f, 215.03f, 158.83f, 214.22f, 157.83f, 214.22f, 139.17f, 214.22f, 139.17f, 195.57f, 139.17f, 195.57f };
static const float kGameCube_9[] = { 139.17f, 195.57f, 139.15f, 195.26f, 139.05f, 194.92f, 138.9f, 194.6f, 138.68f, 194.33f, 138.42f, 194.1f, 138.12f, 193.92f, 137.78f, 193.8f, 137.42f, 193.75f, 123.33f, 193.76f, 122.81f, 193.87f, 122.35f, 194.13f, 121.95f, 194.55f, 121.73f, 195.03f, 121.64f, 195.57f, 121.64f, 214.22f, 130.41f, 222.99f, 139.18f, 214.22f };
static const float kGameCube_10[] = { 121.64f, 231.75f, 121.65f, 250.47f, 121.77f, 251.06f, 122.06f, 251.56f, 122.35f, 251.84f, 122.7f, 252.06f, 123.09f, 252.19f, 123.46f, 252.22f, 137.42f, 252.22f, 138.01f, 252.1f, 138.51f, 251.81f, 138.8f, 251.52f, 138.98f, 251.22f, 139.12f, 250.83f, 139.17f, 250.47f, 139.18f, 231.76f, 130.41f, 222.99f };
static const float kGameCube_11[] = { 121.64f, 214.22f, 102.93f, 214.22f, 102.34f, 214.34f, 101.84f, 214.64f, 101.55f, 214.93f, 101.34f, 215.28f, 101.21f, 215.67f, 101.18f, 216.04f, 101.18f, 230.0f, 101.3f, 230.59f, 101.59f, 231.09f, 101.88f, 231.37f, 102.18f, 231.56f, 102.57f, 231.7f, 102.93f, 231.75f, 121.64f, 231.75f, 130.41f, 222.99f };
static const float kGameCube_12[] = { 139.18f, 231.76f, 157.89f, 231.76f, 158.48f, 231.64f, 158.98f, 231.34f, 159.27f, 231.05f, 159.48f, 230.7f, 159.61f, 230.31f, 159.65f, 229.94f, 159.65f, 215.98f, 159.53f, 215.39f, 159.23f, 214.88f, 158.94f, 214.6f, 158.64f, 214.41f, 158.25f, 214.27f, 157.89f, 214.22f, 139.18f, 214.22f, 130.41f, 222.99f };
static ControllerLayout buildGameCube()
{
	ControllerLayout l;
	l.width = 400.0f;	l.height = 323.75f;
	l.bodyPaths.push_back(mk("MCCLCCCCCCCCLCCLCCCCCCZ", kGameCube_1, 116));
	l.spots.push_back(spot(INPUT_TRIGGER_L, CTL_SHAPE::PATH, 32.11f, 26.52f, 58.86f, 41.1f, 0.0f, "L", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kGameCube_2, 184), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_R, CTL_SHAPE::PATH, 309.03f, 26.52f, 58.86f, 41.18f, 0.0f, "R", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kGameCube_3, 184), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZR, CTL_SHAPE::PATH, 266.98f, 18.47f, 37.29f, 25.45f, 0.0f, "Z", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kGameCube_4, 108), 0u));
	l.spots.push_back(spot(INPUT_THUMB_L, CTL_SHAPE::STICK, 27.86f, 85.28f, 92.31f, 92.31f, 54.85f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_THUMB_R, CTL_SHAPE::STICK, 238.51f, 193.68f, 59.64f, 58.94f, 32.91f, nullptr, mk("MLLLLLLLLZ", kGameCube_5, 18), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::CIRCLE, 186.69f, 116.45f, 26.49f, 26.49f, 0.0f, "S", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 297.93f, 108.16f, 53.51f, 53.51f, 0.0f, "A", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::CIRCLE, 262.27f, 141.61f, 30.64f, 30.64f, 0.0f, "B", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_X, CTL_SHAPE::PATH, 354.96f, 101.49f, 27.52f, 46.0f, 0.0f, "X", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kGameCube_6, 152), 0u));
	l.spots.push_back(spot(INPUT_BTN_Y, CTL_SHAPE::PATH, 288.42f, 77.54f, 46.0f, 27.49f, 0.0f, "Y", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kGameCube_7, 148), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 101.18f, 193.75f, 58.47f, 58.47f, 0.0f, nullptr, mk("MCLCLLCLCLLCLCLLCLCLLLZ", kGameCube_8, 76), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 121.64f, 193.75f, 17.53f, 29.23f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLZ", kGameCube_9, 36), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 121.64f, 222.99f, 17.54f, 29.24f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLZ", kGameCube_10, 34), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 101.18f, 214.22f, 29.23f, 17.53f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLZ", kGameCube_11, 34), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 130.41f, 214.22f, 29.23f, 17.54f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLZ", kGameCube_12, 34), CTL_GHOST));
	return l;
}

static const float kClassic_1[] = { 301.41f, 20.46f, 301.41f, 20.46f, 271.6f, 20.15f, 254.04f, 22.02f, 232.12f, 23.53f, 216.14f, 24.29f, 199.98f, 24.6f, 199.98f, 24.6f, 199.98f, 24.6f, 181.9f, 24.25f, 165.07f, 23.39f, 144.42f, 21.87f, 126.73f, 20.17f, 98.55f, 20.46f, 98.55f, 20.46f, 46.37f, 20.46f, 4.07f, 62.76f, 4.07f, 114.94f, 4.07f, 167.12f, 46.37f, 206.85f, 98.55f, 206.85f, 118.55f, 206.85f, 127.43f, 205.28f, 149.42f, 203.65f, 164.64f, 202.51f, 182.97f, 201.94f, 192.84f, 201.7f, 209.98f, 201.77f, 220.26f, 202.05f, 236.69f, 202.62f, 250.54f, 203.65f, 272.54f, 205.28f, 281.42f, 206.85f, 301.42f, 206.85f, 353.6f, 206.85f, 395.9f, 167.12f, 395.9f, 114.94f, 395.9f, 62.76f, 353.59f, 20.46f, 301.41f, 20.46f };
static const float kClassic_2[] = { 117.42f, 20.58f, 117.56f, 18.87f, 117.63f, 17.1f, 117.46f, 15.33f, 117.07f, 13.6f, 116.79f, 12.76f, 116.07f, 11.14f, 115.15f, 9.63f, 114.03f, 8.25f, 112.75f, 7.03f, 112.04f, 6.48f, 110.54f, 5.54f, 109.75f, 5.14f, 108.93f, 4.79f, 108.09f, 4.5f, 107.24f, 4.27f, 106.37f, 4.09f, 105.49f, 3.97f, 103.72f, 3.9f, 102.83f, 3.95f, 101.95f, 4.06f, 48.38f, 12.54f, 46.65f, 12.94f, 45.81f, 13.21f, 44.19f, 13.94f, 43.42f, 14.37f, 41.97f, 15.4f, 40.66f, 16.6f, 39.53f, 17.97f, 39.04f, 18.71f, 38.19f, 20.26f, 37.55f, 21.92f, 37.32f, 22.77f, 37.14f, 23.64f, 37.02f, 24.52f, 36.95f, 26.29f, 37.0f, 27.18f, 37.11f, 28.06f, 37.28f, 28.93f, 37.78f, 30.63f, 38.5f, 32.25f, 38.94f, 33.02f, 39.97f, 34.47f, 41.17f, 35.77f, 41.83f, 36.36f, 42.54f, 36.91f, 44.06f, 37.95f, 46.83f, 36.08f, 48.85f, 34.72f, 50.86f, 33.36f, 53.04f, 32.27f, 55.22f, 31.17f, 59.59f, 28.98f, 61.77f, 27.89f, 64.09f, 27.08f, 66.41f, 26.27f, 68.75f, 25.46f, 73.43f, 23.84f, 75.87f, 23.34f, 78.32f, 22.84f, 80.78f, 22.33f, 83.25f, 21.83f, 85.73f, 21.32f, 88.26f, 21.15f, 90.81f, 20.98f, 93.37f, 20.81f, 95.96f, 20.63f, 99.96f, 20.45f, 108.01f, 20.46f, 117.27f, 20.57f };
static const float kClassic_3[] = { 356.73f, 37.4f, 358.17f, 36.36f, 359.45f, 35.14f, 360.57f, 33.76f, 361.5f, 32.25f, 362.22f, 30.63f, 362.49f, 29.79f, 362.89f, 28.06f, 363.05f, 26.29f, 362.98f, 24.52f, 362.86f, 23.64f, 362.45f, 21.92f, 361.81f, 20.26f, 360.96f, 18.71f, 360.47f, 17.97f, 359.92f, 17.27f, 359.34f, 16.6f, 358.03f, 15.4f, 356.58f, 14.37f, 355.01f, 13.55f, 354.19f, 13.21f, 353.35f, 12.94f, 351.62f, 12.54f, 298.05f, 4.06f, 296.28f, 3.9f, 295.4f, 3.91f, 294.51f, 3.97f, 293.63f, 4.09f, 291.91f, 4.5f, 290.25f, 5.14f, 288.69f, 5.99f, 287.96f, 6.48f, 287.25f, 7.03f, 285.97f, 8.25f, 284.85f, 9.63f, 283.93f, 11.14f, 283.21f, 12.76f, 282.93f, 13.6f, 282.54f, 15.33f, 282.37f, 17.1f, 282.44f, 18.87f, 282.73f, 20.57f, 288.04f, 20.49f, 297.07f, 20.44f, 304.0f, 20.63f, 306.58f, 20.81f, 309.15f, 20.98f, 311.69f, 21.15f, 314.23f, 21.32f, 316.7f, 21.83f, 319.17f, 22.33f, 321.63f, 22.83f, 324.09f, 23.34f, 326.52f, 23.83f, 328.87f, 24.65f, 331.2f, 25.46f, 333.54f, 26.27f, 335.87f, 27.08f, 338.18f, 27.88f, 340.37f, 28.98f, 342.56f, 30.08f, 344.74f, 31.17f, 346.93f, 32.27f, 349.1f, 33.36f, 351.11f, 34.72f, 353.12f, 36.08f, 355.92f, 37.97f };
static const float kClassic_4[] = { 167.08f, 23.48f, 167.36f, 22.87f, 167.79f, 21.61f, 167.94f, 20.97f, 168.12f, 19.65f, 168.12f, 18.32f, 167.94f, 17.0f, 167.79f, 16.35f, 167.36f, 15.09f, 166.77f, 13.9f, 166.03f, 12.79f, 165.16f, 11.79f, 164.67f, 11.34f, 164.16f, 10.92f, 163.05f, 10.18f, 161.86f, 9.59f, 160.6f, 9.16f, 159.95f, 9.01f, 158.63f, 8.83f, 150.52f, 8.83f, 149.2f, 9.01f, 148.56f, 9.16f, 147.29f, 9.59f, 146.1f, 10.18f, 145.0f, 10.92f, 143.99f, 11.79f, 143.54f, 12.28f, 143.12f, 12.79f, 142.38f, 13.9f, 141.79f, 15.09f, 141.36f, 16.35f, 141.11f, 17.65f, 141.02f, 18.98f, 141.11f, 20.31f, 141.36f, 21.61f, 145.11f, 21.93f, 164.39f, 23.34f };
static const float kClassic_5[] = { 255.95f, 21.84f, 256.15f, 21.83f, 256.25f, 19.65f, 256.25f, 18.32f, 256.07f, 17.0f, 255.93f, 16.35f, 255.5f, 15.09f, 254.91f, 13.9f, 254.56f, 13.34f, 253.75f, 12.28f, 253.29f, 11.79f, 252.29f, 10.92f, 251.19f, 10.18f, 249.99f, 9.59f, 249.37f, 9.36f, 248.73f, 9.16f, 247.43f, 8.9f, 246.1f, 8.81f, 238.66f, 8.83f, 237.34f, 9.01f, 236.69f, 9.16f, 235.43f, 9.59f, 234.83f, 9.86f, 233.67f, 10.53f, 232.62f, 11.34f, 232.13f, 11.79f, 231.25f, 12.79f, 230.52f, 13.9f, 229.93f, 15.09f, 229.5f, 16.35f, 229.24f, 17.65f, 229.17f, 18.32f, 229.17f, 19.65f, 229.35f, 20.97f, 229.69f, 22.25f, 229.96f, 23.63f, 230.27f, 23.61f, 232.85f, 23.48f, 254.04f, 22.02f, 255.86f, 21.85f };
static const float kClassic_6[] = { 145.68f, 123.96f, 125.68f, 132.0f, 116.8f, 151.28f, 125.05f, 172.57f, 145.67f, 181.04f, 166.3f, 172.57f, 174.55f, 151.28f, 165.67f, 132.0f, 145.68f, 123.96f };
static const float kClassic_7[] = { 248.54f, 123.96f, 228.55f, 132.0f, 219.67f, 151.28f, 227.92f, 172.57f, 248.54f, 181.04f, 269.16f, 172.57f, 277.41f, 151.28f, 268.53f, 132.0f, 248.54f, 123.96f };
static const float kClassic_8[] = { 72.53f, 60.03f, 89.09f, 60.03f, 89.09f, 80.92f, 109.98f, 80.92f, 109.98f, 97.48f, 89.09f, 97.48f, 89.09f, 118.37f, 72.53f, 118.37f, 72.53f, 97.48f, 51.65f, 97.48f, 51.65f, 80.92f, 72.53f, 80.92f };
static const float kClassic_9[] = { 72.53f, 60.03f, 89.09f, 60.03f, 89.09f, 80.92f, 80.81f, 89.2f, 72.53f, 80.92f };
static const float kClassic_10[] = { 72.53f, 97.48f, 80.81f, 89.2f, 89.09f, 97.48f, 89.09f, 118.37f, 72.53f, 118.37f };
static const float kClassic_11[] = { 51.65f, 80.92f, 72.53f, 80.92f, 80.81f, 89.2f, 72.53f, 97.48f, 51.65f, 97.48f };
static const float kClassic_12[] = { 109.98f, 80.92f, 109.98f, 97.48f, 89.09f, 97.48f, 80.81f, 89.2f, 89.09f, 80.92f };
static ControllerLayout buildClassic()
{
	ControllerLayout l;
	l.width = 400.0f;	l.height = 211.53f;
	l.bodyPaths.push_back(mk("MCLCCLCCCCCLCCCCZ", kClassic_1, 80));
	l.spots.push_back(spot(INPUT_TRIGGER_L, CTL_SHAPE::PATH, 36.95f, 3.9f, 80.68f, 33.92f, 0.0f, "L", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kClassic_2, 140), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_R, CTL_SHAPE::PATH, 282.37f, 3.9f, 80.68f, 33.93f, 0.0f, "R", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kClassic_3, 134), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZL, CTL_SHAPE::PATH, 141.02f, 8.83f, 27.1f, 14.64f, 0.0f, "ZL", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kClassic_4, 74), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZR, CTL_SHAPE::PATH, 229.17f, 8.81f, 27.08f, 14.8f, 0.0f, "ZR", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kClassic_5, 80), 0u));
	l.spots.push_back(spot(INPUT_THUMB_L, CTL_SHAPE::STICK, 116.8f, 123.96f, 57.75f, 57.08f, 38.92f, nullptr, mk("MLLLLLLLLZ", kClassic_6, 18), 0u));
	l.spots.push_back(spot(INPUT_THUMB_R, CTL_SHAPE::STICK, 219.67f, 123.96f, 57.75f, 57.08f, 38.92f, nullptr, mk("MLLLLLLLLZ", kClassic_7, 18), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 51.65f, 60.03f, 58.34f, 58.34f, 0.0f, nullptr, mk("MLLLLLLLLLLLZ", kClassic_8, 24), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 72.53f, 60.03f, 16.56f, 29.17f, 0.0f, nullptr, mk("MLLLLZ", kClassic_9, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 72.53f, 89.2f, 16.56f, 29.17f, 0.0f, nullptr, mk("MLLLLZ", kClassic_10, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 51.65f, 80.91f, 29.17f, 16.56f, 0.0f, nullptr, mk("MLLLLZ", kClassic_11, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 80.81f, 80.91f, 29.17f, 16.56f, 0.0f, nullptr, mk("MLLLLZ", kClassic_12, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_MINUS, CTL_SHAPE::CIRCLE, 160.14f, 80.34f, 19.66f, 19.66f, 0.0f, "-", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_HOME, CTL_SHAPE::CIRCLE, 188.34f, 80.34f, 19.66f, 19.66f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::CIRCLE, 217.36f, 80.34f, 19.66f, 19.66f, 0.0f, "+", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_X, CTL_SHAPE::CIRCLE, 299.02f, 46.41f, 32.2f, 32.2f, 0.0f, "X", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_Y, CTL_SHAPE::CIRCLE, 261.86f, 74.2f, 32.2f, 32.2f, 0.0f, "Y", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 336.51f, 74.2f, 32.2f, 32.2f, 0.0f, "A", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::CIRCLE, 299.02f, 102.0f, 32.2f, 32.2f, 0.0f, "B", GuiPath(), 0u));
	return l;
}

static const float kWiimote_1[] = { 26.85f, 7.16f, 76.96f, 7.16f, 87.83f, 7.16f, 96.64f, 15.97f, 96.64f, 26.85f, 96.64f, 373.15f, 96.64f, 384.03f, 87.83f, 392.84f, 76.96f, 392.84f, 26.85f, 392.84f, 15.97f, 392.84f, 7.16f, 384.03f, 7.16f, 373.15f, 7.16f, 26.85f, 7.16f, 15.97f, 15.97f, 7.16f, 26.85f, 7.16f };
static const float kWiimote_2[] = { 45.64f, 54.59f, 58.17f, 54.59f, 58.17f, 68.01f, 71.59f, 68.01f, 71.59f, 80.54f, 58.17f, 80.54f, 58.17f, 93.96f, 45.64f, 93.96f, 45.64f, 80.54f, 32.21f, 80.54f, 32.21f, 68.01f, 45.64f, 68.01f, 45.64f, 54.59f };
static const float kWiimote_3[] = { 45.64f, 54.59f, 58.17f, 54.59f, 58.17f, 68.01f, 51.9f, 74.27f, 45.64f, 68.01f };
static const float kWiimote_4[] = { 58.17f, 80.54f, 58.17f, 93.96f, 45.64f, 93.96f, 45.64f, 80.54f, 51.9f, 74.27f };
static const float kWiimote_5[] = { 51.9f, 74.27f, 45.64f, 80.54f, 32.21f, 80.54f, 32.21f, 68.01f, 45.64f, 68.01f };
static const float kWiimote_6[] = { 58.17f, 68.01f, 71.59f, 68.01f, 71.59f, 80.54f, 58.17f, 80.54f, 51.9f, 74.27f };
static const float kWiimote_7[] = { 105.57f, 121.12f, 105.42f, 119.96f, 105.29f, 119.38f, 104.91f, 118.27f, 104.67f, 117.74f, 104.39f, 117.23f, 103.74f, 116.25f, 103.37f, 115.8f, 102.54f, 114.97f, 102.09f, 114.6f, 101.12f, 113.95f, 100.07f, 113.43f, 98.96f, 113.06f, 98.39f, 112.92f, 97.23f, 112.77f, 96.64f, 112.75f, 96.64f, 170.02f, 97.23f, 170.0f, 98.39f, 169.85f, 98.96f, 169.72f, 100.07f, 169.34f, 101.12f, 168.82f, 101.62f, 168.51f, 102.54f, 167.8f, 103.37f, 166.97f, 103.74f, 166.52f, 104.39f, 165.55f, 104.91f, 164.5f, 105.29f, 163.39f, 105.42f, 162.82f, 105.57f, 161.66f, 105.59f, 161.07f };
static ControllerLayout buildWiimote()
{
	ControllerLayout l;
	l.width = 114.54f;	l.height = 400.0f;
	l.bodyPaths.push_back(mk("MLCLCLCLCZ", kWiimote_1, 34));
	l.spots.push_back(spot(0, CTL_SHAPE::CIRCLE, 21.03f, 24.61f, 13.42f, 13.42f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 32.21f, 54.59f, 39.37f, 39.37f, 0.0f, nullptr, mk("MLLLLLLLLLLLLZ", kWiimote_2, 26), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 45.64f, 54.59f, 12.53f, 19.69f, 0.0f, nullptr, mk("MLLLLZ", kWiimote_3, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 45.64f, 74.27f, 12.53f, 19.69f, 0.0f, nullptr, mk("MLLLLZ", kWiimote_4, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 32.21f, 68.01f, 19.69f, 12.53f, 0.0f, nullptr, mk("MLLLLZ", kWiimote_5, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 51.9f, 68.01f, 19.69f, 12.53f, 0.0f, nullptr, mk("MLLLLZ", kWiimote_6, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 38.48f, 119.91f, 26.85f, 26.85f, 0.0f, "A", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_MINUS, CTL_SHAPE::CIRCLE, 19.24f, 189.26f, 13.42f, 13.42f, 0.0f, "-", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_HOME, CTL_SHAPE::CIRCLE, 43.85f, 187.92f, 16.11f, 16.11f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::CIRCLE, 71.14f, 189.26f, 13.42f, 13.42f, 0.0f, "+", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_1, CTL_SHAPE::CIRCLE, 42.51f, 285.01f, 18.79f, 18.79f, 0.0f, "1", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_2, CTL_SHAPE::CIRCLE, 42.51f, 319.02f, 18.79f, 18.79f, 0.0f, "2", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::PATH, 96.64f, 112.75f, 8.95f, 57.27f, 0.0f, "B", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kWiimote_7, 64), 0u));
	return l;
}

static const float kWiimoteSideways_1[] = { 7.16f, 85.91f, 7.16f, 35.79f, 7.16f, 24.92f, 15.97f, 16.11f, 26.85f, 16.11f, 373.15f, 16.11f, 384.03f, 16.11f, 392.84f, 24.92f, 392.84f, 35.79f, 392.84f, 85.91f, 392.84f, 96.78f, 384.03f, 105.59f, 373.15f, 105.59f, 26.85f, 105.59f, 15.97f, 105.59f, 7.16f, 96.78f, 7.16f, 85.91f };
static const float kWiimoteSideways_2[] = { 54.59f, 67.11f, 54.59f, 54.59f, 68.01f, 54.59f, 68.01f, 41.16f, 80.54f, 41.16f, 80.54f, 54.59f, 93.96f, 54.59f, 93.96f, 67.11f, 80.54f, 67.11f, 80.54f, 80.54f, 68.01f, 80.54f, 68.01f, 67.11f, 54.59f, 67.11f };
static const float kWiimoteSideways_3[] = { 68.01f, 54.59f, 68.01f, 41.16f, 80.54f, 41.16f, 80.54f, 54.59f, 74.27f, 60.85f };
static const float kWiimoteSideways_4[] = { 80.54f, 67.11f, 80.54f, 80.54f, 68.01f, 80.54f, 68.01f, 67.11f, 74.27f, 60.85f };
static const float kWiimoteSideways_5[] = { 54.59f, 67.11f, 54.59f, 54.59f, 68.01f, 54.59f, 74.27f, 60.85f, 68.01f, 67.11f };
static const float kWiimoteSideways_6[] = { 80.54f, 54.59f, 93.96f, 54.59f, 93.96f, 67.11f, 80.54f, 67.11f, 74.27f, 60.85f };
static const float kWiimoteSideways_7[] = { 121.12f, 7.18f, 119.96f, 7.33f, 119.38f, 7.46f, 118.27f, 7.84f, 117.74f, 8.08f, 117.23f, 8.36f, 116.25f, 9.01f, 115.8f, 9.38f, 114.97f, 10.21f, 114.6f, 10.66f, 113.95f, 11.63f, 113.43f, 12.68f, 113.06f, 13.79f, 112.92f, 14.36f, 112.77f, 15.53f, 112.75f, 16.11f, 170.02f, 16.11f, 170.0f, 15.53f, 169.85f, 14.36f, 169.72f, 13.79f, 169.34f, 12.68f, 168.82f, 11.63f, 168.51f, 11.13f, 167.8f, 10.21f, 166.97f, 9.38f, 166.52f, 9.01f, 165.55f, 8.36f, 164.5f, 7.84f, 163.39f, 7.46f, 162.82f, 7.33f, 161.66f, 7.18f, 161.07f, 7.16f };
static ControllerLayout buildWiimoteSideways()
{
	ControllerLayout l;
	l.width = 400.0f;	l.height = 114.54f;
	l.bodyPaths.push_back(mk("MLCLCLCLCZ", kWiimoteSideways_1, 34));
	l.spots.push_back(spot(0, CTL_SHAPE::CIRCLE, 24.61f, 78.3f, 13.42f, 13.42f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 54.59f, 41.16f, 39.37f, 39.37f, 0.0f, nullptr, mk("MLLLLLLLLLLLLZ", kWiimoteSideways_2, 26), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 68.01f, 41.16f, 12.53f, 19.69f, 0.0f, nullptr, mk("MLLLLZ", kWiimoteSideways_3, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 68.01f, 60.85f, 12.53f, 19.69f, 0.0f, nullptr, mk("MLLLLZ", kWiimoteSideways_4, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 54.59f, 54.59f, 19.69f, 12.53f, 0.0f, nullptr, mk("MLLLLZ", kWiimoteSideways_5, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 74.27f, 54.59f, 19.69f, 12.53f, 0.0f, nullptr, mk("MLLLLZ", kWiimoteSideways_6, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 119.91f, 47.43f, 26.85f, 26.85f, 0.0f, "A", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_MINUS, CTL_SHAPE::CIRCLE, 189.26f, 80.09f, 13.42f, 13.42f, 0.0f, "-", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_HOME, CTL_SHAPE::CIRCLE, 187.92f, 52.8f, 16.11f, 16.11f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::CIRCLE, 189.26f, 28.19f, 13.42f, 13.42f, 0.0f, "+", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_1, CTL_SHAPE::CIRCLE, 285.01f, 51.45f, 18.79f, 18.79f, 0.0f, "1", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_2, CTL_SHAPE::CIRCLE, 319.02f, 51.45f, 18.79f, 18.79f, 0.0f, "2", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::PATH, 112.75f, 7.16f, 57.27f, 8.95f, 0.0f, "B", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kWiimoteSideways_7, 64), 0u));
	return l;
}

static const float kWiimoteNunchuk_1[] = { 66.22f, 69.8f, 102.01f, 69.8f, 119.91f, 105.59f, 119.91f, 154.81f, 119.91f, 221.92f, 102.01f, 277.4f, 82.33f, 304.25f, 75.17f, 316.78f, 57.27f, 316.78f, 50.11f, 304.25f, 30.43f, 277.4f, 12.53f, 221.92f, 12.53f, 154.81f, 12.53f, 105.59f, 30.43f, 69.8f, 66.22f, 69.8f };
static const float kWiimoteNunchuk_2[] = { 66.22f, 126.17f, 94.89f, 142.73f, 94.89f, 175.84f, 66.22f, 192.39f, 37.55f, 175.84f, 37.55f, 142.73f, 66.22f, 126.17f };
static const float kWiimoteNunchuk_3[] = { 56.38f, 89.49f, 76.06f, 89.49f, 76.61f, 89.5f, 77.69f, 89.65f, 78.2f, 89.77f, 79.2f, 90.12f, 79.67f, 90.34f, 80.57f, 90.86f, 80.98f, 91.16f, 81.76f, 91.84f, 82.44f, 92.62f, 82.74f, 93.04f, 83.26f, 93.93f, 83.48f, 94.4f, 83.83f, 95.4f, 83.95f, 95.92f, 84.1f, 96.99f, 84.12f, 97.54f, 84.1f, 98.09f, 83.95f, 99.16f, 83.83f, 99.68f, 83.48f, 100.67f, 83.02f, 101.6f, 82.44f, 102.46f, 81.76f, 103.23f, 80.98f, 103.91f, 80.13f, 104.49f, 79.2f, 104.96f, 78.2f, 105.31f, 77.16f, 105.52f, 76.06f, 105.59f, 56.38f, 105.59f, 55.82f, 105.57f, 54.75f, 105.43f, 54.23f, 105.31f, 53.24f, 104.96f, 52.31f, 104.49f, 51.87f, 104.22f, 51.06f, 103.59f, 50.33f, 102.86f, 50.0f, 102.46f, 49.42f, 101.6f, 49.17f, 101.15f, 48.77f, 100.18f, 48.61f, 99.68f, 48.4f, 98.63f, 48.32f, 97.54f, 48.34f, 96.99f, 48.49f, 95.92f, 48.77f, 94.89f, 48.96f, 94.4f, 49.42f, 93.47f, 49.7f, 93.04f, 50.33f, 92.22f, 51.06f, 91.49f, 51.45f, 91.16f, 52.31f, 90.59f, 52.77f, 90.34f, 53.73f, 89.93f, 54.75f, 89.65f, 55.28f, 89.56f };
static const float kWiimoteNunchuk_4[] = { 12.39f, 168.42f, 11.15f, 168.75f, 9.96f, 169.24f, 8.85f, 169.89f, 8.32f, 170.26f, 7.36f, 171.11f, 6.51f, 172.08f, 6.13f, 172.61f, 5.49f, 173.72f, 4.99f, 174.91f, 4.81f, 175.53f, 4.55f, 176.79f, 4.47f, 178.08f, 4.49f, 230.62f, 4.55f, 231.27f, 4.81f, 232.53f, 4.99f, 233.15f, 5.49f, 234.34f, 6.13f, 235.45f, 6.51f, 235.97f, 7.36f, 236.94f, 7.83f, 237.38f, 8.32f, 237.79f, 9.4f, 238.51f, 9.96f, 238.81f, 11.15f, 239.3f, 11.77f, 239.49f, 13.03f, 239.74f, 13.67f, 239.8f, 14.96f, 239.8f, 15.61f, 239.74f, 16.87f, 239.49f, 17.49f, 239.3f, 18.68f, 238.81f, 19.24f, 238.51f, 20.31f, 237.79f, 21.28f, 236.94f, 21.83f, 236.34f, 20.09f, 228.67f, 18.42f, 220.28f, 16.92f, 211.62f, 15.94f, 204.95f, 15.34f, 200.43f, 14.8f, 195.86f, 14.31f, 191.22f, 13.88f, 186.53f, 13.18f, 176.99f, 12.78f, 168.36f };
static const float kWiimoteNunchuk_5[] = { 184.34f, 7.16f, 234.45f, 7.16f, 245.32f, 7.16f, 254.14f, 15.97f, 254.14f, 26.85f, 254.14f, 373.15f, 254.14f, 384.03f, 245.32f, 392.84f, 234.45f, 392.84f, 184.34f, 392.84f, 173.47f, 392.84f, 164.65f, 384.03f, 164.65f, 373.15f, 164.65f, 26.85f, 164.65f, 15.97f, 173.47f, 7.16f, 184.34f, 7.16f };
static const float kWiimoteNunchuk_6[] = { 203.13f, 54.59f, 215.66f, 54.59f, 215.66f, 68.01f, 229.08f, 68.01f, 229.08f, 80.54f, 215.66f, 80.54f, 215.66f, 93.96f, 203.13f, 93.96f, 203.13f, 80.54f, 189.71f, 80.54f, 189.71f, 68.01f, 203.13f, 68.01f, 203.13f, 54.59f };
static const float kWiimoteNunchuk_7[] = { 203.13f, 54.59f, 215.66f, 54.59f, 215.66f, 68.01f, 209.4f, 74.27f, 203.13f, 68.01f };
static const float kWiimoteNunchuk_8[] = { 215.66f, 80.54f, 215.66f, 93.96f, 203.13f, 93.96f, 203.13f, 80.54f, 209.4f, 74.27f };
static const float kWiimoteNunchuk_9[] = { 209.4f, 74.27f, 203.13f, 80.54f, 189.71f, 80.54f, 189.71f, 68.01f, 203.13f, 68.01f };
static const float kWiimoteNunchuk_10[] = { 215.66f, 68.01f, 229.08f, 68.01f, 229.08f, 80.54f, 215.66f, 80.54f, 209.4f, 74.27f };
static const float kWiimoteNunchuk_11[] = { 263.07f, 121.12f, 262.92f, 119.96f, 262.78f, 119.38f, 262.41f, 118.27f, 262.17f, 117.74f, 261.89f, 117.23f, 261.23f, 116.25f, 260.87f, 115.8f, 260.04f, 114.97f, 259.59f, 114.6f, 258.61f, 113.95f, 257.57f, 113.43f, 256.46f, 113.06f, 255.88f, 112.92f, 254.72f, 112.77f, 254.14f, 112.75f, 254.14f, 170.02f, 254.72f, 170.0f, 255.88f, 169.85f, 256.46f, 169.72f, 257.57f, 169.34f, 258.61f, 168.82f, 259.59f, 168.17f, 260.04f, 167.8f, 260.87f, 166.97f, 261.57f, 166.05f, 261.89f, 165.55f, 262.41f, 164.5f, 262.78f, 163.39f, 262.92f, 162.82f, 263.07f, 161.66f, 263.09f, 161.07f };
static ControllerLayout buildWiimoteNunchuk()
{
	ControllerLayout l;
	l.width = 277.4f;	l.height = 400.0f;
	l.bodyPaths.push_back(mk("MCCCCCZ", kWiimoteNunchuk_1, 32));
	l.bodyPaths.push_back(mk("MLCLCLCLCZ", kWiimoteNunchuk_5, 34));
	l.spots.push_back(spot(INPUT_THUMB_L, CTL_SHAPE::STICK, 37.55f, 126.17f, 57.34f, 66.22f, 39.37f, nullptr, mk("MLLLLLLZ", kWiimoteNunchuk_2, 14), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_L, CTL_SHAPE::PATH, 48.32f, 89.49f, 35.79f, 16.11f, 0.0f, "C", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kWiimoteNunchuk_3, 122), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZL, CTL_SHAPE::PATH, 4.47f, 168.36f, 17.35f, 71.45f, 0.0f, "Z", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kWiimoteNunchuk_4, 96), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::CIRCLE, 178.52f, 24.61f, 13.42f, 13.42f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 189.71f, 54.59f, 39.37f, 39.37f, 0.0f, nullptr, mk("MLLLLLLLLLLLLZ", kWiimoteNunchuk_6, 26), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 203.13f, 54.59f, 12.53f, 19.69f, 0.0f, nullptr, mk("MLLLLZ", kWiimoteNunchuk_7, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 203.13f, 74.27f, 12.53f, 19.69f, 0.0f, nullptr, mk("MLLLLZ", kWiimoteNunchuk_8, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 189.71f, 68.01f, 19.69f, 12.53f, 0.0f, nullptr, mk("MLLLLZ", kWiimoteNunchuk_9, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 209.4f, 68.01f, 19.69f, 12.53f, 0.0f, nullptr, mk("MLLLLZ", kWiimoteNunchuk_10, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 195.97f, 119.91f, 26.85f, 26.85f, 0.0f, "A", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_MINUS, CTL_SHAPE::CIRCLE, 176.73f, 189.26f, 13.42f, 13.42f, 0.0f, "-", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_HOME, CTL_SHAPE::CIRCLE, 201.34f, 187.92f, 16.11f, 16.11f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::CIRCLE, 228.64f, 189.26f, 13.42f, 13.42f, 0.0f, "+", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_1, CTL_SHAPE::CIRCLE, 200.0f, 285.01f, 18.79f, 18.79f, 0.0f, "1", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_2, CTL_SHAPE::CIRCLE, 200.0f, 319.02f, 18.79f, 18.79f, 0.0f, "2", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::PATH, 254.14f, 112.75f, 8.95f, 57.27f, 0.0f, "B", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kWiimoteNunchuk_11, 64), 0u));
	return l;
}

static const float kDS4_1[] = { 46.29f, 42.84f, 23.76f, 58.54f, 18.48f, 81.42f, 16.85f, 93.81f, 15.16f, 106.19f, 4.47f, 187.68f, 3.65f, 197.36f, 2.77f, 206.97f, 2.23f, 243.05f, 35.26f, 248.26f, 68.36f, 253.54f, 85.69f, 223.49f, 85.69f, 223.49f, 107.95f, 184.84f, 157.83f, 223.49f, 181.52f, 170.9f, 181.52f, 170.9f, 218.54f, 170.9f, 218.54f, 170.9f, 242.23f, 223.49f, 292.12f, 184.84f, 314.38f, 223.49f, 314.38f, 223.49f, 331.78f, 253.54f, 364.81f, 248.26f, 397.83f, 243.05f, 397.29f, 206.97f, 396.48f, 197.36f, 395.6f, 187.68f, 384.91f, 106.19f, 383.21f, 93.81f, 381.59f, 81.42f, 376.38f, 58.54f, 353.77f, 42.84f, 280.54f, 41.08f, 119.53f, 41.08f, 46.29f, 42.84f };
static const float kDS4_2[] = { 48.39f, 42.64f, 48.4f, 32.64f, 48.49f, 31.66f, 48.79f, 30.35f, 49.11f, 29.59f, 49.51f, 28.92f, 50.0f, 28.34f, 50.28f, 28.09f, 50.9f, 27.64f, 51.62f, 27.28f, 52.42f, 27.01f, 53.32f, 26.83f, 54.82f, 26.73f, 111.68f, 26.73f, 112.76f, 26.78f, 114.25f, 27.01f, 115.56f, 27.45f, 116.33f, 27.85f, 117.34f, 28.62f, 118.17f, 29.59f, 118.83f, 30.76f, 119.17f, 31.66f, 119.53f, 33.16f, 119.53f, 41.08f };
static const float kDS4_3[] = { 351.74f, 42.64f, 351.73f, 32.64f, 351.64f, 31.66f, 351.34f, 30.35f, 350.84f, 29.25f, 350.39f, 28.62f, 349.86f, 28.09f, 349.23f, 27.64f, 348.52f, 27.28f, 347.71f, 27.01f, 346.82f, 26.83f, 345.31f, 26.73f, 288.46f, 26.73f, 287.37f, 26.78f, 285.89f, 27.01f, 285.0f, 27.28f, 283.81f, 27.85f, 282.8f, 28.62f, 281.96f, 29.59f, 281.31f, 30.76f, 280.97f, 31.66f, 280.61f, 33.16f, 280.61f, 41.08f };
static const float kDS4_4[] = { 54.82f, 26.73f, 58.21f, 12.86f, 58.62f, 10.64f, 58.98f, 9.32f, 59.64f, 7.6f, 60.16f, 6.61f, 60.74f, 5.75f, 61.4f, 5.03f, 62.12f, 4.44f, 62.9f, 3.98f, 63.75f, 3.65f, 64.67f, 3.45f, 65.65f, 3.38f, 102.2f, 3.38f, 103.19f, 3.45f, 103.66f, 3.53f, 104.12f, 3.65f, 104.99f, 3.98f, 105.81f, 4.44f, 106.2f, 4.72f, 106.93f, 5.37f, 107.28f, 5.75f, 107.92f, 6.61f, 108.23f, 7.09f, 109.05f, 8.71f, 109.75f, 10.64f, 110.32f, 12.86f, 114.38f, 26.73f };
static const float kDS4_5[] = { 345.31f, 26.73f, 341.93f, 12.86f, 341.52f, 10.64f, 340.96f, 8.71f, 340.5f, 7.6f, 339.98f, 6.61f, 339.39f, 5.75f, 338.74f, 5.03f, 338.02f, 4.44f, 337.23f, 3.98f, 336.38f, 3.65f, 335.47f, 3.45f, 334.48f, 3.38f, 297.94f, 3.38f, 296.95f, 3.45f, 296.48f, 3.53f, 295.57f, 3.8f, 295.14f, 3.98f, 294.73f, 4.19f, 293.94f, 4.72f, 293.56f, 5.03f, 292.86f, 5.75f, 292.21f, 6.61f, 291.91f, 7.09f, 291.08f, 8.71f, 290.38f, 10.64f, 289.81f, 12.86f, 285.75f, 26.73f };
static const float kDS4_6[] = { 66.94f, 65.99f, 67.41f, 59.29f, 76.62f, 59.29f, 80.54f, 59.29f, 84.47f, 59.29f, 93.67f, 59.29f, 94.15f, 65.99f, 94.42f, 69.58f, 93.2f, 80.34f, 91.78f, 83.18f, 90.56f, 85.62f, 85.14f, 89.68f, 81.9f, 91.98f, 81.83f, 92.12f, 81.69f, 92.18f, 81.62f, 92.25f, 81.29f, 92.39f, 80.88f, 92.52f, 80.54f, 92.52f, 80.41f, 92.52f, 80.27f, 92.52f, 80.14f, 92.45f, 80.0f, 92.45f, 79.8f, 92.39f, 79.66f, 92.32f, 79.53f, 92.25f, 79.32f, 92.12f, 79.19f, 91.98f, 77.02f, 90.49f, 70.59f, 85.75f, 69.31f, 83.18f, 67.88f, 80.34f, 66.67f, 69.58f, 66.94f, 65.99f, 50.96f, 109.17f, 44.2f, 108.7f, 44.2f, 99.49f, 44.2f, 95.57f, 44.2f, 91.64f, 44.2f, 82.44f, 50.96f, 81.96f, 51.3f, 81.96f, 51.71f, 81.96f, 52.12f, 81.96f, 55.97f, 81.96f, 65.38f, 82.98f, 68.16f, 84.33f, 70.73f, 85.62f, 75.47f, 92.05f, 76.95f, 94.21f, 77.09f, 94.42f, 77.23f, 94.55f, 77.29f, 94.75f, 77.43f, 95.03f, 77.5f, 95.36f, 77.5f, 95.63f, 77.5f, 95.91f, 77.43f, 96.24f, 77.29f, 96.51f, 77.16f, 96.65f, 77.09f, 96.79f, 76.95f, 96.92f, 75.74f, 98.68f, 70.86f, 105.45f, 68.16f, 106.8f, 65.38f, 108.22f, 55.97f, 109.24f, 52.12f, 109.24f, 51.71f, 109.24f, 51.3f, 109.24f, 50.96f, 109.17f, 94.15f, 125.14f, 93.67f, 131.91f, 84.47f, 131.91f, 80.54f, 131.91f, 80.54f, 131.91f, 80.54f, 131.91f, 76.62f, 131.91f, 67.41f, 131.91f, 66.94f, 125.14f, 66.67f, 121.56f, 67.88f, 110.8f, 69.31f, 107.95f, 70.59f, 105.38f, 77.02f, 100.71f, 79.19f, 99.15f, 79.26f, 99.09f, 79.39f, 99.02f, 79.53f, 98.95f, 79.66f, 98.82f, 79.8f, 98.75f, 80.0f, 98.75f, 80.54f, 98.54f, 81.08f, 98.61f, 81.62f, 98.95f, 81.76f, 99.02f, 81.9f, 99.09f, 81.96f, 99.22f, 85.21f, 101.52f, 90.56f, 105.58f, 91.78f, 107.95f, 93.2f, 110.8f, 94.42f, 121.56f, 94.15f, 125.14f, 108.97f, 109.24f, 105.11f, 109.24f, 95.7f, 108.22f, 92.93f, 106.8f, 90.22f, 105.45f, 85.35f, 98.68f, 84.13f, 96.92f, 83.99f, 96.85f, 83.93f, 96.72f, 83.86f, 96.58f, 83.65f, 96.24f, 83.59f, 95.84f, 83.59f, 95.43f, 83.65f, 95.16f, 83.72f, 94.96f, 83.79f, 94.69f, 83.93f, 94.55f, 83.99f, 94.35f, 84.13f, 94.21f, 85.69f, 92.05f, 90.36f, 85.62f, 92.93f, 84.33f, 95.7f, 82.98f, 105.11f, 81.96f, 108.97f, 81.96f, 109.37f, 81.96f, 109.78f, 81.96f, 110.12f, 81.96f, 116.82f, 82.44f, 116.89f, 91.51f, 116.89f, 95.5f, 116.89f, 95.5f, 116.89f, 95.57f, 116.89f, 95.57f, 116.89f, 99.49f, 116.89f, 108.7f, 110.12f, 109.17f, 109.78f, 109.24f, 109.37f, 109.24f, 108.97f, 109.24f };
static const float kDS4_7[] = { 66.94f, 65.99f, 67.41f, 59.29f, 76.62f, 59.29f, 80.54f, 59.29f, 84.47f, 59.29f, 93.67f, 59.29f, 94.15f, 65.99f, 94.42f, 69.58f, 93.2f, 80.34f, 91.78f, 83.18f, 90.56f, 85.62f, 85.14f, 89.68f, 81.9f, 91.98f, 81.83f, 92.12f, 81.69f, 92.18f, 81.62f, 92.25f, 81.29f, 92.39f, 80.88f, 92.52f, 80.54f, 92.52f, 80.41f, 92.52f, 80.27f, 92.52f, 80.14f, 92.45f, 80.0f, 92.45f, 79.8f, 92.39f, 79.66f, 92.32f, 79.53f, 92.25f, 79.32f, 92.12f, 79.19f, 91.98f, 77.02f, 90.49f, 70.59f, 85.75f, 69.31f, 83.18f, 67.88f, 80.34f, 66.67f, 69.58f, 66.94f, 65.99f };
static const float kDS4_8[] = { 94.15f, 125.14f, 93.67f, 131.91f, 84.47f, 131.91f, 80.54f, 131.91f, 80.54f, 131.91f, 80.54f, 131.91f, 76.62f, 131.91f, 67.41f, 131.91f, 66.94f, 125.14f, 66.67f, 121.56f, 67.88f, 110.8f, 69.31f, 107.95f, 70.59f, 105.38f, 77.02f, 100.71f, 79.19f, 99.15f, 79.26f, 99.09f, 79.39f, 99.02f, 79.53f, 98.95f, 79.66f, 98.82f, 79.8f, 98.75f, 80.0f, 98.75f, 80.54f, 98.54f, 81.08f, 98.61f, 81.62f, 98.95f, 81.76f, 99.02f, 81.9f, 99.09f, 81.96f, 99.22f, 85.21f, 101.52f, 90.56f, 105.58f, 91.78f, 107.95f, 93.2f, 110.8f, 94.42f, 121.56f, 94.15f, 125.14f };
static const float kDS4_9[] = { 50.96f, 109.17f, 44.2f, 108.7f, 44.2f, 99.49f, 44.2f, 95.57f, 44.2f, 91.64f, 44.2f, 82.44f, 50.96f, 81.96f, 51.3f, 81.96f, 51.71f, 81.96f, 52.12f, 81.96f, 55.97f, 81.96f, 65.38f, 82.98f, 68.16f, 84.33f, 70.73f, 85.62f, 75.47f, 92.05f, 76.95f, 94.21f, 77.09f, 94.42f, 77.23f, 94.55f, 77.29f, 94.75f, 77.43f, 95.03f, 77.5f, 95.36f, 77.5f, 95.63f, 77.5f, 95.91f, 77.43f, 96.24f, 77.29f, 96.51f, 77.16f, 96.65f, 77.09f, 96.79f, 76.95f, 96.92f, 75.74f, 98.68f, 70.86f, 105.45f, 68.16f, 106.8f, 65.38f, 108.22f, 55.97f, 109.24f, 52.12f, 109.24f, 51.71f, 109.24f, 51.3f, 109.24f, 50.96f, 109.17f };
static const float kDS4_10[] = { 108.97f, 109.24f, 105.11f, 109.24f, 95.7f, 108.22f, 92.93f, 106.8f, 90.22f, 105.45f, 85.35f, 98.68f, 84.13f, 96.92f, 83.99f, 96.85f, 83.93f, 96.72f, 83.86f, 96.58f, 83.65f, 96.24f, 83.59f, 95.84f, 83.59f, 95.43f, 83.65f, 95.16f, 83.72f, 94.96f, 83.79f, 94.69f, 83.93f, 94.55f, 83.99f, 94.35f, 84.13f, 94.21f, 85.69f, 92.05f, 90.36f, 85.62f, 92.93f, 84.33f, 95.7f, 82.98f, 105.11f, 81.96f, 108.97f, 81.96f, 109.37f, 81.96f, 109.78f, 81.96f, 110.12f, 81.96f, 116.82f, 82.44f, 116.89f, 91.51f, 116.89f, 95.5f, 116.89f, 95.5f, 116.89f, 95.57f, 116.89f, 95.57f, 116.89f, 99.49f, 116.89f, 108.7f, 110.12f, 109.17f, 109.78f, 109.24f, 109.37f, 109.24f, 108.97f, 109.24f };
static const float kDS4_11[] = { 174.62f, 97.94f, 174.59f, 98.38f, 174.44f, 99.02f, 174.17f, 99.61f, 173.79f, 100.12f, 173.33f, 100.56f, 172.79f, 100.9f, 172.18f, 101.13f, 171.75f, 101.22f, 156.89f, 101.25f, 156.44f, 101.22f, 155.8f, 101.07f, 155.03f, 100.68f, 154.4f, 100.12f, 153.92f, 99.42f, 153.64f, 98.6f, 153.57f, 97.94f, 153.57f, 91.03f, 153.6f, 90.58f, 153.75f, 89.95f, 154.14f, 89.2f, 154.4f, 88.87f, 154.86f, 88.45f, 155.6f, 88.03f, 156.44f, 87.81f, 171.3f, 87.78f, 171.97f, 87.85f, 172.59f, 88.03f, 173.33f, 88.45f, 173.93f, 89.03f, 174.36f, 89.75f, 174.55f, 90.37f, 174.62f, 91.03f };
static const float kDS4_12[] = { 228.56f, 101.59f, 227.95f, 101.66f, 227.29f, 101.59f, 226.66f, 101.4f, 226.1f, 101.09f, 225.46f, 100.53f, 225.09f, 100.01f, 224.82f, 99.43f, 224.67f, 98.79f, 224.64f, 89.81f, 224.68f, 89.31f, 224.87f, 88.6f, 225.09f, 88.16f, 225.36f, 87.75f, 225.68f, 87.39f, 226.04f, 87.08f, 226.65f, 86.72f, 227.09f, 86.56f, 227.55f, 86.48f, 228.02f, 86.48f, 228.49f, 86.57f, 246.29f, 91.37f, 246.71f, 91.55f, 247.07f, 91.81f, 247.37f, 92.13f, 247.59f, 92.51f, 247.76f, 93.05f, 247.78f, 93.61f, 247.68f, 94.22f, 247.48f, 94.79f, 247.18f, 95.31f, 246.8f, 95.76f, 246.35f, 96.14f, 245.84f, 96.44f, 245.46f, 96.59f, 244.87f, 96.72f };
static ControllerLayout buildDS4()
{
	ControllerLayout l;
	l.width = 400.0f;	l.height = 251.78f;
	l.bodyPaths.push_back(mk("MCCCCLCLCLCCCCLLLZ", kDS4_1, 74));
	l.spots.push_back(spot(INPUT_TRIGGER_L, CTL_SHAPE::PATH, 48.39f, 26.73f, 71.13f, 15.91f, 0.0f, "L1", mk("MLLLLLLLLLLLLLLLLLLLLLLLZ", kDS4_2, 48), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_R, CTL_SHAPE::PATH, 280.61f, 26.73f, 71.13f, 15.91f, 0.0f, "R1", mk("MLLLLLLLLLLLLLLLLLLLLLLZ", kDS4_3, 46), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZL, CTL_SHAPE::PATH, 54.82f, 3.38f, 59.56f, 23.35f, 0.0f, "L2", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kDS4_4, 56), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZR, CTL_SHAPE::PATH, 285.75f, 3.38f, 59.56f, 23.35f, 0.0f, "R2", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kDS4_5, 56), 0u));
	l.spots.push_back(spot(INPUT_THUMB_L, CTL_SHAPE::STICK, 111.4f, 122.44f, 54.82f, 54.82f, 32.49f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_THUMB_R, CTL_SHAPE::STICK, 234.11f, 122.44f, 54.82f, 54.82f, 32.49f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 44.2f, 59.29f, 72.69f, 72.62f, 0.0f, nullptr, mk("MCCCCCCCCCCCZMCCCCCCCCCCCCZMCLLCCCCCCCCCZMCCCCCCCCCCCCCZ", kDS4_6, 288), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 66.9f, 59.29f, 27.28f, 33.23f, 0.0f, nullptr, mk("MCCCCCCCCCCCZ", kDS4_7, 68), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 66.9f, 98.63f, 27.28f, 33.28f, 0.0f, nullptr, mk("MCLLCCCCCCCCCZ", kDS4_8, 66), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 44.2f, 81.96f, 33.3f, 27.28f, 0.0f, nullptr, mk("MCCCCCCCCCCCCZ", kDS4_9, 74), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 83.59f, 81.96f, 33.3f, 27.28f, 0.0f, nullptr, mk("MCCCCCCCCCCCCCZ", kDS4_10, 80), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_Y, CTL_SHAPE::CIRCLE, 302.4f, 45.69f, 32.49f, 32.49f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_X, CTL_SHAPE::CIRCLE, 270.86f, 77.29f, 32.49f, 32.49f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::CIRCLE, 333.94f, 77.29f, 32.49f, 32.49f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 302.4f, 108.83f, 32.49f, 32.49f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_MINUS, CTL_SHAPE::PATH, 153.57f, 87.78f, 21.05f, 13.47f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kDS4_11, 66), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::PATH, 224.64f, 86.48f, 23.14f, 15.18f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kDS4_12, 72), 0u));
	l.spots.push_back(spot(INPUT_BTN_HOME, CTL_SHAPE::CIRCLE, 179.09f, 109.51f, 41.96f, 41.96f, 0.0f, nullptr, GuiPath(), 0u));
	return l;
}

static const float kXbox360_1[] = { 347.49f, 320.74f, 291.04f, 320.74f, 297.79f, 198.73f, 213.18f, 198.73f, 108.49f, 198.73f, 133.51f, 321.87f, 57.12f, 321.87f, -33.71f, 321.87f, -6.22f, 14.52f, 115.05f, 14.52f, 115.05f, 14.52f, 149.3f, 31.37f, 195.59f, 37.19f, 223.01f, 37.19f, 267.42f, 31.24f, 295.45f, 15.05f, 295.45f, 15.05f, 395.85f, 15.05f, 437.99f, 320.74f, 347.49f, 320.74f };
static const float kXbox360_2[] = { 16.12f, 122.74f, 15.12f, 116.59f, 14.58f, 112.56f, 14.33f, 109.4f, 14.34f, 106.6f, 14.44f, 104.95f, 14.65f, 102.77f, 14.93f, 100.61f, 15.35f, 97.93f, 16.47f, 91.58f, 17.03f, 88.96f, 17.78f, 85.84f, 18.6f, 82.76f, 19.35f, 80.19f, 20.98f, 75.16f, 22.41f, 71.24f, 23.4f, 68.83f, 24.64f, 65.98f, 25.96f, 63.17f, 27.33f, 60.39f, 29.29f, 56.81f, 30.84f, 54.18f, 32.18f, 52.03f, 33.85f, 49.5f, 35.0f, 47.87f, 36.49f, 45.87f, 38.02f, 43.91f, 39.59f, 42.0f, 41.2f, 40.13f, 44.22f, 36.91f, 45.96f, 35.2f, 47.74f, 33.55f, 49.57f, 31.97f, 51.43f, 30.45f, 53.34f, 28.99f, 54.89f, 27.87f, 56.87f, 26.54f, 58.86f, 25.28f, 60.92f, 24.11f, 62.58f, 23.22f, 64.68f, 22.19f, 66.38f, 21.43f, 68.1f, 20.73f, 71.55f, 19.43f, 73.3f, 18.84f, 75.07f, 18.33f, 76.41f, 18.02f, 77.77f, 17.78f, 80.07f, 17.57f, 81.07f, 17.56f, 82.66f, 17.64f, 85.96f, 18.03f, 90.97f, 18.86f, 86.57f, 20.66f, 82.27f, 22.74f, 78.09f, 25.11f, 74.02f, 27.75f, 70.05f, 30.65f, 66.2f, 33.81f, 62.46f, 37.2f, 58.83f, 40.83f, 55.31f, 44.68f, 51.9f, 48.75f, 48.6f, 53.02f, 45.42f, 57.48f, 42.35f, 62.13f, 39.39f, 66.96f, 36.55f, 71.95f, 33.82f, 77.1f, 31.2f, 82.39f, 28.7f, 87.82f, 26.31f, 93.38f, 24.04f, 99.05f, 21.89f, 104.84f, 19.85f, 110.72f, 17.93f, 116.69f };
static const float kXbox360_3[] = { 315.65f, 19.33f, 320.76f, 18.49f, 323.29f, 18.2f, 325.21f, 18.14f, 326.09f, 18.19f, 327.35f, 18.36f, 328.6f, 18.61f, 330.23f, 19.04f, 331.84f, 19.56f, 333.42f, 20.16f, 335.79f, 21.14f, 337.35f, 21.84f, 338.89f, 22.6f, 340.79f, 23.63f, 342.66f, 24.74f, 344.51f, 25.91f, 345.98f, 26.9f, 347.78f, 28.2f, 349.55f, 29.58f, 351.28f, 31.03f, 352.98f, 32.53f, 354.64f, 34.1f, 357.53f, 37.09f, 360.28f, 40.27f, 362.03f, 42.46f, 363.46f, 44.34f, 364.86f, 46.28f, 366.2f, 48.27f, 368.76f, 52.34f, 370.93f, 56.17f, 372.3f, 58.81f, 373.61f, 61.49f, 375.44f, 65.59f, 377.28f, 70.28f, 378.48f, 73.63f, 379.29f, 76.07f, 380.75f, 81.04f, 382.01f, 86.08f, 382.69f, 89.17f, 383.19f, 91.78f, 384.21f, 98.09f, 384.78f, 102.34f, 384.98f, 104.49f, 385.07f, 106.13f, 385.1f, 107.78f, 385.07f, 109.46f, 384.81f, 112.58f, 384.25f, 116.56f, 383.21f, 122.61f, 381.43f, 116.59f, 379.55f, 110.65f, 377.58f, 104.8f, 375.5f, 99.05f, 373.34f, 93.41f, 371.07f, 87.88f, 368.72f, 82.49f, 366.28f, 77.22f, 363.74f, 72.11f, 361.11f, 67.14f, 358.4f, 62.35f, 355.6f, 57.73f, 352.72f, 53.29f, 349.75f, 49.04f, 346.69f, 45.0f, 343.56f, 41.17f, 340.34f, 37.56f, 337.04f, 34.19f, 333.67f, 31.05f, 330.22f, 28.17f, 326.69f, 25.55f, 323.08f, 23.19f, 319.4f, 21.12f };
static const float kXbox360_4[] = { 115.12f, 14.58f, 117.95f, 9.91f, 118.91f, 8.52f, 119.73f, 7.56f, 120.61f, 6.72f, 121.46f, 6.09f, 122.81f, 5.28f, 124.15f, 4.68f, 125.7f, 4.16f, 127.14f, 3.85f, 128.84f, 3.62f, 130.61f, 3.56f, 133.12f, 3.71f, 135.09f, 3.94f, 137.15f, 4.33f, 139.49f, 4.95f, 143.25f, 6.19f, 145.84f, 7.17f, 148.82f, 8.44f, 152.18f, 10.03f, 157.2f, 12.59f, 159.96f, 14.1f, 165.38f, 17.29f, 168.72f, 19.35f, 176.35f, 24.28f, 178.14f, 25.55f, 179.57f, 26.71f, 181.1f, 28.09f, 182.77f, 29.76f, 184.9f, 32.02f, 188.76f, 36.25f, 183.59f, 35.41f, 178.54f, 34.49f, 173.63f, 33.5f, 168.86f, 32.46f, 164.23f, 31.37f, 159.77f, 30.24f, 155.47f, 29.09f, 151.34f, 27.92f, 143.64f, 25.55f, 140.07f, 24.38f, 133.57f, 22.12f, 127.94f, 20.01f, 123.25f, 18.14f, 119.56f, 16.59f };
static const float kXbox360_5[] = { 229.5f, 36.19f, 235.06f, 29.72f, 236.6f, 28.06f, 237.99f, 26.71f, 239.32f, 25.55f, 240.96f, 24.28f, 247.55f, 19.61f, 250.5f, 17.58f, 255.29f, 14.43f, 259.0f, 12.11f, 261.05f, 10.95f, 264.95f, 8.83f, 266.85f, 7.94f, 270.37f, 6.42f, 272.92f, 5.52f, 275.12f, 4.87f, 276.3f, 4.62f, 278.39f, 4.33f, 280.39f, 4.2f, 282.1f, 4.27f, 283.78f, 4.52f, 285.04f, 4.86f, 286.27f, 5.32f, 287.32f, 5.84f, 288.9f, 6.79f, 289.72f, 7.42f, 290.57f, 8.25f, 291.2f, 8.97f, 292.15f, 10.24f, 295.38f, 15.05f, 292.92f, 16.38f, 290.26f, 17.7f, 286.7f, 19.37f, 282.27f, 21.31f, 276.99f, 23.44f, 274.05f, 24.55f, 267.57f, 26.84f, 264.05f, 27.99f, 260.33f, 29.13f, 256.44f, 30.26f, 252.37f, 31.36f, 248.13f, 32.43f, 243.71f, 33.46f, 239.14f, 34.43f, 234.4f, 35.35f };
static const float kXbox360_6[] = { 132.91f, 117.32f, 150.5f, 117.32f, 150.5f, 133.11f, 166.29f, 133.11f, 166.29f, 150.7f, 150.5f, 150.7f, 150.5f, 166.49f, 132.91f, 166.49f, 132.91f, 150.7f, 117.12f, 150.7f, 117.12f, 133.11f, 132.91f, 133.11f, 132.91f, 117.32f };
static const float kXbox360_7[] = { 132.91f, 117.32f, 150.5f, 117.32f, 150.5f, 133.11f, 141.71f, 141.91f, 132.91f, 133.11f };
static const float kXbox360_8[] = { 150.5f, 150.7f, 150.5f, 166.49f, 132.91f, 166.49f, 132.91f, 150.7f, 141.71f, 141.91f };
static const float kXbox360_9[] = { 141.71f, 141.91f, 132.91f, 150.7f, 117.12f, 150.7f, 117.12f, 133.11f, 132.91f, 133.11f };
static const float kXbox360_10[] = { 150.5f, 133.11f, 166.29f, 133.11f, 166.29f, 150.7f, 150.5f, 150.7f, 141.71f, 141.91f };
static ControllerLayout buildXbox360()
{
	ControllerLayout l;
	l.width = 400.0f;	l.height = 324.41f;
	l.bodyPaths.push_back(mk("MCCCCLCCZ", kXbox360_1, 40));
	l.spots.push_back(spot(INPUT_TRIGGER_L, CTL_SHAPE::PATH, 14.33f, 17.56f, 76.64f, 105.18f, 0.0f, "LB", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kXbox360_2, 152), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_R, CTL_SHAPE::PATH, 315.65f, 18.14f, 69.45f, 104.47f, 0.0f, "RB", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kXbox360_3, 144), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZL, CTL_SHAPE::PATH, 115.12f, 3.56f, 73.65f, 32.69f, 0.0f, "LT", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kXbox360_4, 90), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_ZR, CTL_SHAPE::PATH, 229.5f, 4.2f, 65.89f, 31.99f, 0.0f, "RT", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kXbox360_5, 90), 0u));
	l.spots.push_back(spot(INPUT_THUMB_L, CTL_SHAPE::STICK, 69.7f, 50.23f, 52.71f, 52.71f, 32.24f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_THUMB_R, CTL_SHAPE::STICK, 218.93f, 117.99f, 52.71f, 52.71f, 32.24f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::CIRCLE, 106.22f, 106.49f, 70.9f, 70.9f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 117.12f, 117.32f, 49.16f, 49.16f, 0.0f, nullptr, mk("MLLLLLLLLLLLLZ", kXbox360_6, 26), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 132.91f, 117.32f, 17.59f, 24.58f, 0.0f, nullptr, mk("MLLLLZ", kXbox360_7, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 132.91f, 141.91f, 17.59f, 24.58f, 0.0f, nullptr, mk("MLLLLZ", kXbox360_8, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 117.12f, 133.11f, 24.58f, 17.59f, 0.0f, nullptr, mk("MLLLLZ", kXbox360_9, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 141.71f, 133.11f, 24.58f, 17.59f, 0.0f, nullptr, mk("MLLLLZ", kXbox360_10, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_X, CTL_SHAPE::CIRCLE, 272.37f, 85.42f, 26.62f, 26.62f, 0.0f, "X", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_Y, CTL_SHAPE::CIRCLE, 294.85f, 56.92f, 26.62f, 26.62f, 0.0f, "Y", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 297.66f, 107.89f, 26.62f, 26.62f, 0.0f, "A", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::CIRCLE, 320.13f, 79.8f, 26.62f, 26.62f, 0.0f, "B", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_MINUS, CTL_SHAPE::CIRCLE, 157.86f, 60.07f, 22.47f, 22.47f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_HOME, CTL_SHAPE::CIRCLE, 185.95f, 52.37f, 31.57f, 31.57f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::CIRCLE, 225.95f, 60.07f, 22.47f, 22.47f, 0.0f, nullptr, GuiPath(), 0u));
	return l;
}

static const float kUsb_1[] = { 84.47f, 183.97f, 39.53f, 183.97f, 3.07f, 147.51f, 3.07f, 102.57f, 3.07f, 57.63f, 39.53f, 21.24f, 84.47f, 21.24f, 314.99f, 21.24f, 359.93f, 21.24f, 396.39f, 57.63f, 396.39f, 102.57f, 396.39f, 147.51f, 359.93f, 183.97f, 314.99f, 183.97f, 290.88f, 183.97f, 269.25f, 173.49f, 254.36f, 156.86f, 145.11f, 156.86f, 130.22f, 173.49f, 108.58f, 183.97f, 84.47f, 183.97f };
static const float kUsb_2[] = { 17.43f, 57.23f, 16.74f, 54.13f, 16.22f, 51.01f, 15.88f, 47.89f, 15.71f, 44.78f, 15.72f, 41.69f, 15.9f, 38.63f, 16.26f, 35.62f, 16.8f, 32.66f, 17.52f, 29.78f, 18.43f, 26.98f, 19.51f, 24.28f, 20.78f, 21.68f, 22.23f, 19.2f, 23.86f, 16.85f, 25.68f, 14.65f, 27.69f, 12.61f, 29.89f, 10.74f, 32.28f, 9.05f, 34.85f, 7.55f, 37.62f, 6.26f, 40.58f, 5.18f, 43.73f, 4.34f, 47.08f, 3.75f, 50.62f, 3.41f, 117.46f, 3.41f, 118.9f, 3.45f, 120.24f, 3.58f, 121.48f, 3.79f, 122.63f, 4.08f, 123.68f, 4.45f, 124.64f, 4.89f, 125.52f, 5.4f, 126.32f, 5.97f, 127.05f, 6.61f, 127.7f, 7.3f, 128.28f, 8.05f, 128.79f, 8.86f, 129.24f, 9.71f, 129.63f, 10.61f, 129.97f, 11.55f, 130.26f, 12.53f, 130.5f, 13.54f, 130.85f, 15.67f, 131.05f, 17.89f, 131.15f, 21.37f, 84.87f, 21.37f, 81.5f, 21.44f, 78.16f, 21.65f, 74.83f, 21.99f, 71.54f, 22.47f, 68.28f, 23.08f, 65.05f, 23.82f, 61.87f, 24.69f, 58.73f, 25.68f, 55.64f, 26.8f, 52.6f, 28.04f, 49.61f, 29.41f, 46.68f, 30.89f, 43.81f, 32.49f, 41.01f, 34.2f, 38.28f, 36.03f, 35.63f, 37.96f, 33.04f, 40.01f, 30.54f, 42.17f, 28.13f, 44.42f, 25.8f, 46.79f, 23.56f, 49.25f, 21.42f, 51.81f, 19.37f, 54.47f };
static const float kUsb_3[] = { 381.9f, 57.23f, 382.59f, 54.13f, 383.11f, 51.01f, 383.45f, 47.89f, 383.62f, 44.78f, 383.62f, 41.69f, 383.43f, 38.63f, 383.07f, 35.62f, 382.53f, 32.66f, 381.81f, 29.78f, 380.91f, 26.98f, 379.82f, 24.28f, 378.56f, 21.68f, 377.11f, 19.2f, 375.47f, 16.85f, 373.65f, 14.65f, 371.64f, 12.61f, 369.44f, 10.74f, 367.06f, 9.05f, 364.48f, 7.55f, 361.71f, 6.26f, 358.75f, 5.18f, 355.6f, 4.34f, 352.26f, 3.75f, 348.71f, 3.41f, 281.87f, 3.41f, 280.43f, 3.45f, 279.09f, 3.58f, 277.85f, 3.79f, 276.71f, 4.08f, 275.65f, 4.45f, 274.69f, 4.89f, 273.81f, 5.4f, 273.01f, 5.97f, 272.29f, 6.61f, 271.64f, 7.3f, 271.06f, 8.05f, 270.54f, 8.86f, 270.09f, 9.71f, 269.7f, 10.61f, 269.36f, 11.55f, 269.07f, 12.53f, 268.83f, 13.54f, 268.48f, 15.67f, 268.28f, 17.89f, 268.18f, 21.37f, 314.46f, 21.37f, 317.83f, 21.44f, 321.18f, 21.65f, 324.5f, 21.99f, 327.79f, 22.47f, 331.06f, 23.08f, 334.28f, 23.82f, 337.46f, 24.69f, 340.6f, 25.68f, 343.7f, 26.8f, 346.74f, 28.04f, 349.72f, 29.41f, 352.65f, 30.89f, 355.52f, 32.49f, 358.32f, 34.2f, 361.05f, 36.03f, 363.71f, 37.96f, 366.29f, 40.01f, 368.79f, 42.17f, 371.21f, 44.42f, 373.53f, 46.79f, 375.77f, 49.25f, 377.92f, 51.81f, 379.96f, 54.47f };
static const float kUsb_4[] = { 70.92f, 61.9f, 98.03f, 61.9f, 98.03f, 89.02f, 125.14f, 89.02f, 125.14f, 116.19f, 98.03f, 116.19f, 98.03f, 143.24f, 70.92f, 143.24f, 70.92f, 116.19f, 43.81f, 116.19f, 43.81f, 89.02f, 70.92f, 89.02f, 70.92f, 61.9f };
static const float kUsb_5[] = { 70.92f, 61.9f, 98.03f, 61.9f, 98.03f, 89.02f, 84.47f, 102.57f, 70.92f, 89.02f };
static const float kUsb_6[] = { 98.1f, 116.19f, 98.03f, 116.19f, 98.03f, 143.24f, 70.92f, 143.24f, 70.92f, 116.19f, 70.85f, 116.19f, 84.47f, 102.57f };
static const float kUsb_7[] = { 84.47f, 102.57f, 70.85f, 116.19f, 43.81f, 116.19f, 43.81f, 89.02f, 70.92f, 89.02f };
static const float kUsb_8[] = { 98.03f, 89.02f, 125.14f, 89.02f, 125.14f, 116.19f, 98.1f, 116.19f, 84.47f, 102.57f };
static const float kUsb_9[] = { 164.87f, 135.43f, 195.06f, 115.86f, 195.8f, 115.25f, 196.13f, 114.92f, 196.72f, 114.21f, 197.42f, 113.02f, 197.87f, 111.75f, 198.04f, 110.86f, 198.1f, 109.96f, 197.98f, 108.6f, 197.59f, 107.26f, 197.19f, 106.41f, 196.94f, 106.0f, 196.67f, 105.6f, 196.05f, 104.88f, 195.37f, 104.25f, 194.22f, 103.5f, 193.39f, 103.13f, 192.09f, 102.77f, 190.74f, 102.66f, 189.83f, 102.73f, 188.48f, 103.05f, 187.6f, 103.41f, 187.18f, 103.64f, 156.99f, 123.14f, 156.58f, 123.42f, 155.84f, 124.06f, 155.5f, 124.41f, 154.91f, 125.15f, 154.66f, 125.55f, 154.23f, 126.39f, 153.8f, 127.72f, 153.64f, 129.11f, 153.68f, 130.05f, 153.74f, 130.52f, 153.96f, 131.45f, 154.12f, 131.9f, 154.54f, 132.79f, 154.79f, 133.22f, 155.38f, 134.02f, 156.06f, 134.71f, 156.42f, 135.02f, 157.21f, 135.56f, 157.62f, 135.78f, 158.48f, 136.16f, 159.84f, 136.5f, 161.24f, 136.58f, 162.17f, 136.48f, 162.64f, 136.38f, 163.56f, 136.09f, 164.01f, 135.9f };
static const float kUsb_10[] = { 219.17f, 135.43f, 249.42f, 115.86f, 249.82f, 115.57f, 250.57f, 114.93f, 250.9f, 114.59f, 251.48f, 113.83f, 251.73f, 113.43f, 252.15f, 112.59f, 252.57f, 111.26f, 252.73f, 109.87f, 252.69f, 108.93f, 252.62f, 108.47f, 252.39f, 107.54f, 252.23f, 107.09f, 251.81f, 106.2f, 251.55f, 105.78f, 251.27f, 105.37f, 250.63f, 104.63f, 249.91f, 103.99f, 249.53f, 103.71f, 248.71f, 103.23f, 247.85f, 102.87f, 246.49f, 102.54f, 245.1f, 102.47f, 244.16f, 102.57f, 243.24f, 102.8f, 242.33f, 103.16f, 241.47f, 103.64f, 211.29f, 123.14f, 210.87f, 123.42f, 210.13f, 124.06f, 209.79f, 124.41f, 209.2f, 125.15f, 208.95f, 125.55f, 208.52f, 126.39f, 208.1f, 127.72f, 207.93f, 129.11f, 207.97f, 130.05f, 208.03f, 130.52f, 208.25f, 131.45f, 208.41f, 131.9f, 208.83f, 132.79f, 209.08f, 133.22f, 209.67f, 134.02f, 210.35f, 134.71f, 210.71f, 135.02f, 211.5f, 135.56f, 211.91f, 135.78f, 212.77f, 136.16f, 214.13f, 136.5f, 215.53f, 136.58f, 216.46f, 136.48f, 216.93f, 136.38f, 217.85f, 136.09f, 218.3f, 135.9f };
static ControllerLayout buildUsb()
{
	ControllerLayout l;
	l.width = 400.0f;	l.height = 186.98f;
	l.bodyPaths.push_back(mk("MCCLCCCLCZ", kUsb_1, 42));
	l.spots.push_back(spot(INPUT_TRIGGER_L, CTL_SHAPE::PATH, 15.71f, 3.41f, 115.44f, 53.82f, 0.0f, "L", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kUsb_2, 140), 0u));
	l.spots.push_back(spot(INPUT_TRIGGER_R, CTL_SHAPE::PATH, 268.18f, 3.41f, 115.44f, 53.82f, 0.0f, "R", mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kUsb_3, 140), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::CIRCLE, 30.12f, 48.21f, 108.71f, 108.71f, 0.0f, nullptr, GuiPath(), 0u));
	l.spots.push_back(spot(0, CTL_SHAPE::PATH, 43.81f, 61.9f, 81.34f, 81.34f, 0.0f, nullptr, mk("MLLLLLLLLLLLLZ", kUsb_4, 26), 0u));
	l.spots.push_back(spot(INPUT_BTN_UP, CTL_SHAPE::PATH, 70.92f, 61.9f, 27.11f, 40.67f, 0.0f, nullptr, mk("MLLLLZ", kUsb_5, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_DOWN, CTL_SHAPE::PATH, 70.85f, 102.57f, 27.25f, 40.67f, 0.0f, nullptr, mk("MLLLLLLZ", kUsb_6, 14), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_LEFT, CTL_SHAPE::PATH, 43.81f, 89.02f, 40.67f, 27.18f, 0.0f, nullptr, mk("MLLLLZ", kUsb_7, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_RIGHT, CTL_SHAPE::PATH, 84.47f, 89.02f, 40.67f, 27.18f, 0.0f, nullptr, mk("MLLLLZ", kUsb_8, 10), CTL_GHOST));
	l.spots.push_back(spot(INPUT_BTN_MINUS, CTL_SHAPE::PATH, 153.64f, 102.66f, 44.46f, 33.91f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kUsb_9, 102), 0u));
	l.spots.push_back(spot(INPUT_BTN_PLUS, CTL_SHAPE::PATH, 207.93f, 102.47f, 44.8f, 34.11f, 0.0f, nullptr, mk("MLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLZ", kUsb_10, 110), 0u));
	l.spots.push_back(spot(INPUT_BTN_Y, CTL_SHAPE::CIRCLE, 278.93f, 80.07f, 31.65f, 31.65f, 0.0f, "Y", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_X, CTL_SHAPE::CIRCLE, 319.2f, 66.38f, 31.65f, 31.65f, 0.0f, "X", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_B, CTL_SHAPE::CIRCLE, 292.62f, 120.33f, 31.65f, 31.65f, 0.0f, "B", GuiPath(), 0u));
	l.spots.push_back(spot(INPUT_BTN_A, CTL_SHAPE::CIRCLE, 332.89f, 107.45f, 31.65f, 31.65f, 0.0f, "A", GuiPath(), 0u));
	return l;
}

const ControllerLayout& WiiUProControllerLayout()
{
	static const ControllerLayout layout = buildWiiUPro();
	return layout;
}

const ControllerLayout& GamePadControllerLayout()
{
	static const ControllerLayout layout = buildGamePad();
	return layout;
}

const ControllerLayout& GameCubeControllerLayout()
{
	static const ControllerLayout layout = buildGameCube();
	return layout;
}

const ControllerLayout& ClassicControllerLayout()
{
	static const ControllerLayout layout = buildClassic();
	return layout;
}

const ControllerLayout& WiimoteControllerLayout()
{
	static const ControllerLayout layout = buildWiimote();
	return layout;
}

const ControllerLayout& WiimoteSidewaysControllerLayout()
{
	static const ControllerLayout layout = buildWiimoteSideways();
	return layout;
}

const ControllerLayout& WiimoteNunchukControllerLayout()
{
	static const ControllerLayout layout = buildWiimoteNunchuk();
	return layout;
}

const ControllerLayout& DS4ControllerLayout()
{
	static const ControllerLayout layout = buildDS4();
	return layout;
}

const ControllerLayout& Xbox360ControllerLayout()
{
	static const ControllerLayout layout = buildXbox360();
	return layout;
}

const ControllerLayout& UsbControllerLayout()
{
	static const ControllerLayout layout = buildUsb();
	return layout;
}
