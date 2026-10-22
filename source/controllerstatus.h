/****************************************************************************
 * Snes9x GX
 *
 * Daryl Borth 2026
 *
 * controllerstatus.h
 *
 * The controller strip at the bottom left of the game menu: an icon for what
 * is connected on each of the four channels, with its battery below, and the
 * GamePad or a USB controller on its own at the far left: always on Wii U, and on
 * Wii only while there is something to show (the other columns close up when
 * there isn't).
 *
 *   ControllerStatus status(trigA);
 *   status.appendTo(window);
 *   ...
 *   status.update();                   // every frame
 *   int ch = status.clicked();         // channel whose mapping to open, or -1
 *
 * A Classic Controller, Pro Controller or Nunchuk is shown instead of the
 * Wiimote it rides on, with that Wiimote's battery below it. Controllers that
 * have no battery to report (GameCube pads, USB controllers) show the icon alone.
 *
 * The left slot shows one of: GamePad, DualShock 4, Xbox 360, other USB
 * controller, in that order; only one even if several are connected. A USB
 * controller that has to be replugged gets a red x.
 *
 * P1 never sits empty while the left slot has something: if no Wiimote, Pro
 * Controller or GameCube pad is on port 1, P1 shows the left slot's controller.
 ***************************************************************************/
#pragma once
#ifndef HW_DOL

#include <stdio.h>

class ControllerStatus
{
	public:
		static const int SLOTS = 4;

		ControllerStatus(GuiTrigger * trig) :
			iconGamepad(controller_gamepad_png),
			iconWiimote(controller_wiimote_png),
			iconNunchuk(controller_nunchuk_png),
			iconClassic(controller_classic_png),
			iconPro(controller_wiiupro_png),
			iconGameCube(controller_gamecube_png),
			iconCharging(controller_charging_png),
			iconDs4(controller_ds4_png),
			iconXbox360(controller_xbox360_png),
			iconUsb(controller_usb_png),
			iconReplug(controller_replug_png),
			screenW(platform->getVideo()->getScreenWidth()),
			numBadges(0),
			leftColumn(LEFT_ALWAYS),
			divider(nullptr)
		{
			// badge[0] is the left slot (GamePad or USB controller), badge[1..4] are P1-P4
			badge[numBadges++] = new Badge(0, nullptr, screenW, trig);
			for(int i = 0; i < SLOTS; i++)
			{
				char txt[3];
				sprintf(txt, "P%d", i + 1);
				badge[numBadges++] = new Badge(i, txt, screenW, trig);
			}
			divider = new GuiShape(1, DIVIDER_H);
			divider->setPaint(GuiPaint().color(MenuRGBA(255, 255, 255, 80)));
			divider->setAlignment(ALIGN_H::CENTRE, ALIGN_V::BOTTOM);

			layout(leftColumn);
			for(int i = 0; i < numBadges; i++)
				apply(*badge[i], Slot());
			if(!leftColumn)
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
			// The left slot: the GamePad (channel 0's DRC profile), else a USB controller, else nothing
			Slot left;
			const InputPadData& drc = controller[0]->getPadData();
			if(drc.hw_connected[INPUT_HW_DRC])
			{
				left.kind = GAMEPAD;
				left.segs = Segments(drc.hw_battery_level[INPUT_HW_DRC]);
				left.charging = drc.hw_charging[INPUT_HW_DRC];
			}
			else
			{
				const InputUsbStatus usb = platform->getInput()->getUsbStatus();
				int n;
				const UsbProfile * profiles = UsbProfiles(n);
				for(int k = 0; k < n; k++)
					if(profiles[k].hw == usb.profile)
						left.kind = profiles[k].kind;
				left.replug = usb.replugRequired;
				// clicking it maps the channel it is playing on
				for(int i = 0; i < SLOTS && left.kind != NONE; i++)
				{
					if(controller[i]->getPadData().hw_connected[usb.profile])
					{
						left.channel = i;
						break;
					}
				}
			}
			const bool column = LEFT_ALWAYS || left.kind != NONE;
			if(column != leftColumn)
				layout(column);
			badge[0]->channel = left.channel;
			if(column)
				apply(*badge[0], left);
			else
				hide(*badge[0]);

			int b = 1;
			for(int i = 0; i < SLOTS; i++)
			{
				const InputPadData& pad = controller[i]->getPadData();
				Slot slot;
				if(pad.hw_connected[INPUT_HW_WUPC]) slot.kind = PRO;
				else if(pad.hw_connected[INPUT_HW_CLASSIC]) slot.kind = CLASSIC;
				else if(pad.hw_connected[INPUT_HW_NUNCHUK]) slot.kind = NUNCHUK;
				else if(pad.hw_connected[INPUT_HW_WIIMOTE]) slot.kind = WIIMOTE;
				else if(pad.hw_connected[INPUT_HW_GAMECUBE]) slot.kind = GAMECUBE;

				// Every Wiimote-based controller reports the battery of the Wiimote it rides on
				if(slot.kind != NONE && slot.kind != GAMECUBE)
					slot.segs = Segments(pad.hw_battery_level[INPUT_HW_WIIMOTE]);

				// a USB controller playing on this channel
				int n;
				const UsbProfile * profiles = UsbProfiles(n);
				for(int k = 0; k < n && slot.kind == NONE; k++)
					if(pad.hw_connected[profiles[k].hw])
						slot.kind = profiles[k].kind;

				// P1 shows the left slot's controller rather than nothing
				if(slot.kind == NONE && i == 0)
				{
					slot = left;
					slot.channel = 0;
				}
				apply(*badge[b++], slot);
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
		enum Kind { NONE, GAMEPAD, WIIMOTE, NUNCHUK, CLASSIC, PRO, GAMECUBE, DUALSHOCK4, XBOX_360, USB_PAD };

		//!What one badge shows
		struct Slot
		{
			Kind kind = NONE;
			int segs = -1;			// battery segments, -1 = no battery
			bool charging = false;
			bool replug = false;	// a USB controller that has to be unplugged and plugged back in
			int channel = 0;		// only used by the left slot
		};

		//!The USB controllers, in the order the strip prefers them
		struct UsbProfile { GuiHardwareProfile hw; Kind kind; };
		static const UsbProfile * UsbProfiles(int & count)
		{
			static const UsbProfile profiles[] = {
				{ INPUT_HW_DS4, DUALSHOCK4 },
				{ INPUT_HW_XBOX360, XBOX_360 },
				{ INPUT_HW_GENERIC_USB, USB_PAD },
			};
			count = sizeof(profiles) / sizeof(profiles[0]);
			return profiles;
		}
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
		static const bool LEFT_ALWAYS = true;	// the Wii U GamePad column is always there
#else
		static const bool LEFT_ALWAYS = false;	// on Wii it only exists while a GamePad or USB controller is connected
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
			GuiImage replug;		// red x on a controller that needs replugging
			GuiShape ring;			// placeholder for an empty slot
			GuiShape body, nub;
			GuiShape * seg[4];
			GuiText * label;

			Badge(int ch, const char * text, int sw, GuiTrigger * trig) :
				cx(0), channel(ch), key(-1), hasLabel(text != nullptr), screenW(sw),
				hit(PITCH, 58), ring(9, 12), body(BODY_W, BODY_H), nub(NUB_W, NUB_H),
				label(nullptr)
			{
				for(int i = 0; i < 4; i++)
					seg[i] = new GuiShape(SEG_W, SEG_H);
				hit.setSelectable(false);
				hit.setTrigger(trig);
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
				w.append(&replug);
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
				replug.setEffect(effect, amount);
				if(label) label->setEffect(effect, amount);
			}
		};

		//!Lays the columns out, with or without the left slot's column
		void layout(bool left)
		{
			leftColumn = left;
			int x = LEFT_MARGIN;
			badge[0]->setCenter(x + PITCH / 2);
			if(left)
				x += PITCH + GAP;
			for(int i = 0; i < SLOTS; i++)
				badge[1 + i]->setCenter(x + PITCH / 2 + i * PITCH);
			divider->setVisible(left);
			divider->setPosition(x - GAP / 2 - screenW / 2, -DIVIDER_BOTTOM);
			for(int i = 0; i < numBadges; i++)	// everything moved: have apply() redo it all
				badge[i]->key = -1;
		}

		//!Takes a badge off screen entirely (the left column on a Wii with nothing to show there)
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
			b.replug.setVisible(false);
			if(b.label) b.label->setVisible(false);
		}

		//!Shows a slot: its icon, and its battery (or the charging look) below
		void apply(Badge& b, const Slot& slot)
		{
			const Kind kind = slot.kind;
			const int segs = slot.segs;
			const bool charging = slot.charging;
			const bool replug = slot.replug && kind != NONE;
			const int key = kind | ((segs + 1) << 4) | (charging << 8) | (replug << 9);
			if(key == b.key)
				return;
			b.key = key;

			const bool present = (kind != NONE);
			b.hit.setVisible(true);
			b.hit.setState(present && !replug ? STATE::DEFAULT : STATE::DISABLED);
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
					case DUALSHOCK4: data = &iconDs4; break;
					case XBOX_360: data = &iconXbox360; break;
					case USB_PAD: data = &iconUsb; break;
					default: break;
				}
				b.icon.setImage(data);
				const int iconBottom = ICON_BOX_BOTTOM + (ICON_BOX_H - b.icon.getHeight()) / 2;
				b.place(&b.icon, b.cx, iconBottom);
				b.icon.setAlpha(replug ? 130 : 255);
				if(replug)
				{
					// the red x sits on the icon's top right corner
					b.replug.setImage(&iconReplug);
					b.place(&b.replug, b.cx + b.icon.getWidth() / 2 - 2, iconBottom + b.icon.getHeight() - 8);
				}
			}
			b.replug.setVisible(present && replug);

			const bool showBattery = present && !replug && (segs >= 0 || charging);
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

		GuiImageData iconGamepad, iconWiimote, iconNunchuk, iconClassic, iconPro, iconGameCube, iconCharging, iconDs4, iconXbox360, iconUsb, iconReplug;
		int screenW;
		int numBadges;
		bool leftColumn;			// whether the left slot's column is laid out
		Badge * badge[SLOTS + 1];	// the left slot (GamePad or USB controller) first, then P1-P4
		GuiShape * divider;
};

#endif
