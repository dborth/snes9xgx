/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * controllerstatus.h
 *
 * The controller strip at the bottom left of the game menu: an icon for what
 * is connected on each of the four channels, with its battery below, and the
 * GamePad on its own at the far left: always on Wii U, and on Wii only while a
 * GamePad is connected (the other columns close up when it isn't).
 *
 *   ControllerStatus status(trigA, &btnSoundOver, &btnSoundClick);
 *   status.appendTo(window);
 *   ...
 *   status.update();                   // every frame
 *   int ch = status.clicked();         // channel whose mapping to open, or -1
 *
 * A Classic Controller, Pro Controller or Nunchuk is shown instead of the
 * Wiimote it rides on, with that Wiimote's battery below it. Controllers that
 * have no battery to report (GameCube pads) show the icon alone.
 ***************************************************************************/
#pragma once
#ifndef HW_DOL

#include <stdio.h>

class ControllerStatus
{
	public:
		static const int SLOTS = 4;

		ControllerStatus(GuiTrigger * trig, GuiSound * soundOver, GuiSound * soundClick) :
			iconGamepad(controller_gamepad_png),
			iconWiimote(controller_wiimote_png),
			iconNunchuk(controller_nunchuk_png),
			iconClassic(controller_classic_png),
			iconPro(controller_wiiupro_png),
			iconGameCube(controller_gamecube_png),
			iconCharging(controller_charging_png),
			screenW(platform->getVideo()->getScreenWidth()),
			numBadges(0),
			drcColumn(DRC_ALWAYS),
			divider(nullptr)
		{
			// badge[0] is the GamePad, badge[1..4] are P1-P4
			badge[numBadges++] = new Badge(0, nullptr, screenW, trig, soundOver, soundClick);
			for(int i = 0; i < SLOTS; i++)
			{
				char txt[3];
				sprintf(txt, "P%d", i + 1);
				badge[numBadges++] = new Badge(i, txt, screenW, trig, soundOver, soundClick);
			}
			divider = new GuiShape(1, DIVIDER_H);
			divider->setPaint(GuiPaint().color(MenuRGBA(255, 255, 255, 80)));
			divider->setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);

			layout(drcColumn);
			for(int i = 0; i < numBadges; i++)
				apply(*badge[i], NONE, -1, false);
			if(!drcColumn)
				hide(*badge[0]);
		}

		~ControllerStatus()
		{
			for(int i = 0; i < numBadges; i++)
				delete badge[i];
			delete divider;
		}
		ControllerStatus(const ControllerStatus&) = delete;
		ControllerStatus& operator=(const ControllerStatus&) = delete;

		void appendTo(GuiWindow& w)
		{
			w.append(divider);
			for(int i = 0; i < numBadges; i++)
				badge[i]->appendTo(w);
		}

		//!Applies an effect (slide in/out) to every part
		void setEffect(int effect, int amount)
		{
			divider->setEffect(effect, amount);
			for(int i = 0; i < numBadges; i++)
				badge[i]->setEffect(effect, amount);
		}

		//!Reads the controllers and updates what is shown. Cheap when nothing changed.
		void update()
		{
			// the GamePad is always channel 0's DRC profile; it has its own column
			const InputPadData& drc = controller[0]->getPadData();
			const bool here = drc.hw_connected[INPUT_HW_DRC];
			const bool column = DRC_ALWAYS || here;
			if(column != drcColumn)
				layout(column);
			if(column)
				apply(*badge[0], here ? GAMEPAD : NONE, here ? Segments(drc.hw_battery_level[INPUT_HW_DRC]) : -1, here && drc.hw_charging[INPUT_HW_DRC]);
			else
				hide(*badge[0]);

			int b = 1;
			for(int i = 0; i < SLOTS; i++)
			{
				const InputPadData& pad = controller[i]->getPadData();
				Kind kind = NONE;
				if(pad.hw_connected[INPUT_HW_WUPC]) kind = PRO;
				else if(pad.hw_connected[INPUT_HW_CLASSIC]) kind = CLASSIC;
				else if(pad.hw_connected[INPUT_HW_NUNCHUK]) kind = NUNCHUK;
				else if(pad.hw_connected[INPUT_HW_WIIMOTE]) kind = WIIMOTE;
				else if(pad.hw_connected[INPUT_HW_GAMECUBE]) kind = GAMECUBE;

				// Every Wiimote-based controller reports the battery of the Wiimote it rides on
				bool hasBattery = (kind != NONE && kind != GAMECUBE);
				apply(*badge[b++], kind, hasBattery ? Segments(pad.hw_battery_level[INPUT_HW_WIIMOTE]) : -1, false);
			}
		}

		//!The channel whose player mapping was clicked, or -1. The GamePad belongs to channel 0.
		int clicked()
		{
			for(int i = 0; i < numBadges; i++)
				if(badge[i]->hit.getState() == STATE::CLICKED)
					return badge[i]->channel;
			return -1;
		}

	private:
		enum Kind { NONE, GAMEPAD, WIIMOTE, NUNCHUK, CLASSIC, PRO, GAMECUBE };
		static const int HIDDEN = -2;			// Badge::key of a badge that is off screen

		// Layout, in design-canvas pixels. Distances are from the bottom of the screen.
		static const int LEFT_MARGIN = 26;		// left edge of the first column
		static const int PITCH = 34;			// column width
		static const int GAP = 6;				// extra space either side of the divider
		static const int ICON_BOX_H = 30;		// icons are centered in a box this tall...
		static const int ICON_BOX_BOTTOM = 58;	// ...whose bottom edge is this high
		static const int BATTERY_CY = 44;		// battery's vertical center
		static const int LABEL_BOTTOM = 20;
		static const int DIVIDER_H = 64;
		static const int DIVIDER_BOTTOM = 22;

		// Battery, drawn on whole pixels so nothing is anti-aliased into mush at this size. The casing is a 1px
		// outline, with a 1px gap before the bars, so the bars sit exactly 2px in from the casing's outer edge:
		//   width  = 1 + 1 + (4 bars * 4px + 3 gaps * 1px) + 1 + 1 = 23
		//   height = 1 + 1 + 5px bar + 1 + 1 = 9
		static const int BODY_W = 23, BODY_H = 9;
		static const int BORDER = 1;
		static const int NUB_W = 2, NUB_H = 3;	// the terminal: odd height, so it centers on the odd-height body
		static const int SEG_W = 4, SEG_H = 5, SEG_STEP = 5;
		static const int SEG_INSET = BORDER + 1;	// casing outer edge to the first bar, on both axes
		static const int BATT_LEFT = -12;		// casing's left edge, from the column center (23 + nub 2 = 25 wide)
		static const int BATT_BOTTOM = BATTERY_CY - BODY_H / 2;

#ifdef __WIIU__
		static const bool DRC_ALWAYS = true;		// the Wii U GamePad column is always there
#else
		static const bool DRC_ALWAYS = false;	// on Wii it only exists while a GamePad is connected
#endif

		static int Segments(uint32_t level)
		{
			int s = (int)(level * 4 / 100);
			return s > 4 ? 4 : s;
		}

		static PixelColor White(int a = 255) { return MenuRGBA(255, 255, 255, a); }
		static PixelColor Amber() { return MenuRGBA(255, 201, 51); }

		struct Badge
		{
			int cx;
			int channel;			// the controller channel clicking it opens the mapping for
			int key;				// what is shown now, so unchanged frames cost nothing
			bool hasLabel;
			int screenW;
			GuiButton hit;
			GuiImage icon;
			GuiImage bolt;
			GuiShape ring;			// placeholder for an empty slot
			GuiShape body, nub;
			GuiShape * seg[4];
			GuiText * label;

			Badge(int ch, const char * text, int sw, GuiTrigger * trig, GuiSound * over, GuiSound * click) :
				cx(0), channel(ch), key(-1), hasLabel(text != nullptr), screenW(sw),
				hit(PITCH, 58), ring(9, 12), body(BODY_W, BODY_H), nub(NUB_W, NUB_H),
				label(nullptr)
			{
				for(int i = 0; i < 4; i++)
					seg[i] = new GuiShape(SEG_W, SEG_H);
				hit.setSelectable(false);
				hit.setTrigger(trig);
				hit.setSoundOver(over);
				hit.setSoundClick(click);
				ring.setShape(SHAPE::ELLIPSE);
				ring.setPaint(GuiPaint().stroke(1.0f, White(90)));

				if(hasLabel)
					label = new GuiText(text, 15, White());
			}
			~Badge()
			{
				for(int i = 0; i < 4; i++) delete seg[i];
				delete label;
			}

			//!Centers an element on a column (odd widths round the left edge down)
			void place(GuiElement * e, int centerX, int bottom)
			{
				e->setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);
				e->setPosition(centerX - screenW / 2, -bottom);
			}

			//!Puts an element's left/bottom edge on exact pixels
			void placeAt(GuiElement * e, int left, int bottom)
			{
				e->setAlignment(ALIGN_H::LEFT, ALIGN_V::BOTTOM);
				e->setPosition(left, -bottom);
			}

			//!Moves the badge to a column. The icon and bolt are placed by apply().
			void setCenter(int centerX)
			{
				cx = centerX;
				place(&hit, cx, 18);
				place(&ring, cx, ICON_BOX_BOTTOM + ICON_BOX_H / 2 - 6);

				const int left = cx + BATT_LEFT;
				placeAt(&body, left, BATT_BOTTOM);
				placeAt(&nub, left + BODY_W, BATT_BOTTOM + (BODY_H - NUB_H) / 2);
				for(int i = 0; i < 4; i++)
					placeAt(seg[i], left + SEG_INSET + i * SEG_STEP, BATT_BOTTOM + SEG_INSET);
				if(label)
					place(label, cx, LABEL_BOTTOM);
			}

			void appendTo(GuiWindow& w)
			{
				w.append(&hit);
				w.append(&ring);
				w.append(&icon);
				w.append(&body);
				w.append(&nub);
				for(int i = 0; i < 4; i++) w.append(seg[i]);
				w.append(&bolt);
				if(label) w.append(label);
			}

			void setEffect(int effect, int amount)
			{
				hit.setEffect(effect, amount);
				ring.setEffect(effect, amount);
				icon.setEffect(effect, amount);
				body.setEffect(effect, amount);
				nub.setEffect(effect, amount);
				for(int i = 0; i < 4; i++) seg[i]->setEffect(effect, amount);
				bolt.setEffect(effect, amount);
				if(label) label->setEffect(effect, amount);
			}
		};

		//!Lays the columns out, with or without the GamePad column at the left
		void layout(bool drc)
		{
			drcColumn = drc;
			int x = LEFT_MARGIN;
			badge[0]->setCenter(x + PITCH / 2);
			if(drc)
				x += PITCH + GAP;
			for(int i = 0; i < SLOTS; i++)
				badge[1 + i]->setCenter(x + PITCH / 2 + i * PITCH);
			divider->setVisible(drc);
			divider->setPosition(x - GAP / 2 - screenW / 2, -DIVIDER_BOTTOM);
			for(int i = 0; i < numBadges; i++)	// everything moved: have apply() redo it all
				badge[i]->key = -1;
		}

		//!Takes a badge off screen entirely (the GamePad column on a Wii with no GamePad)
		void hide(Badge& b)
		{
			if(b.key == HIDDEN)
				return;
			b.key = HIDDEN;
			b.hit.setState(STATE::DISABLED);
			b.hit.setVisible(false);
			b.ring.setVisible(false);
			b.icon.setVisible(false);
			b.body.setVisible(false);
			b.nub.setVisible(false);
			for(int i = 0; i < 4; i++) b.seg[i]->setVisible(false);
			b.bolt.setVisible(false);
			if(b.label) b.label->setVisible(false);
		}

		//!Shows `kind` with `segs` battery segments (-1 = no battery shown), or the charging look
		void apply(Badge& b, Kind kind, int segs, bool charging)
		{
			const int key = kind | ((segs + 1) << 4) | (charging << 8);
			if(key == b.key)
				return;
			b.key = key;

			const bool present = (kind != NONE);
			b.hit.setVisible(true);
			b.hit.setState(present ? STATE::DEFAULT : STATE::DISABLED);
			b.ring.setVisible(!present);
			b.icon.setVisible(present);
			if(b.label)
			{
				b.label->setVisible(true);
				b.label->setAlpha(present ? 255 : 90);
			}

			if(present)
			{
				GuiImageData * data = &iconWiimote;
				switch(kind)
				{
					case GAMEPAD: data = &iconGamepad; break;
					case NUNCHUK: data = &iconNunchuk; break;
					case CLASSIC: data = &iconClassic; break;
					case PRO: data = &iconPro; break;
					case GAMECUBE: data = &iconGameCube; break;
					default: break;
				}
				b.icon.setImage(data);
				b.place(&b.icon, b.cx, ICON_BOX_BOTTOM + (ICON_BOX_H - b.icon.getHeight()) / 2);
			}

			const bool showBattery = present && (segs >= 0 || charging);
			const int lit = charging ? 4 : segs;
			const PixelColor tint = (!charging && segs <= 1) ? Amber() : White(); // low or empty
			b.body.setVisible(showBattery);
			b.nub.setVisible(showBattery);
			if(showBattery)
			{
				b.body.setPaint(GuiPaint().stroke((float)BORDER, tint));
				b.nub.setPaint(GuiPaint().color(tint));
			}
			for(int i = 0; i < 4; i++)
			{
				b.seg[i]->setVisible(showBattery && i < lit);
				if(showBattery && i < lit)
					b.seg[i]->setPaint(GuiPaint().color(tint));
			}
			b.bolt.setVisible(showBattery && charging);
			if(showBattery && charging)
			{
				b.bolt.setImage(&iconCharging);
				// centered on the casing by whole pixels (it is taller than the casing, so it overhangs evenly)
				b.placeAt(&b.bolt, b.cx + BATT_LEFT + (BODY_W - b.bolt.getWidth()) / 2, BATT_BOTTOM + (BODY_H - b.bolt.getHeight()) / 2);
			}
		}

		GuiImageData iconGamepad, iconWiimote, iconNunchuk, iconClassic, iconPro, iconGameCube, iconCharging;
		int screenW;
		int numBadges;
		bool drcColumn;				// whether the GamePad column is laid out
		Badge * badge[SLOTS + 1];	// the GamePad first, then P1-P4
		GuiShape * divider;
};

#endif
