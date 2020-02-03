#include "diablo.h"
#include <ncurses.h>

DEVILUTION_BEGIN_NAMESPACE

int light_table_index;
DWORD sgdwCursWdtOld;
DWORD sgdwCursX;
DWORD sgdwCursY;
BYTE *gpBufStart;
BYTE *gpBufEnd;
DWORD sgdwCursHgt;
DWORD level_cel_block;
DWORD sgdwCursXOld;
DWORD sgdwCursYOld;
char arch_draw_type;
int cel_transparency_active;
int level_piece_id;
DWORD sgdwCursWdt;
void (*DrawPlrProc)(int, int, int, int, int, BYTE *, int, int, int, int);
BYTE sgSaveBack[8192];
DWORD sgdwCursHgtOld;

/* data */

/* used in 1.00 debug */
char *szMonModeAssert[18] = {
	"standing",
	"walking (1)",
	"walking (2)",
	"walking (3)",
	"attacking",
	"getting hit",
	"dying",
	"attacking (special)",
	"fading in",
	"fading out",
	"attacking (ranged)",
	"standing (special)",
	"attacking (special ranged)",
	"delaying",
	"charging",
	"stoned",
	"healing",
	"talking"
};

char *szPlrModeAssert[12] = {
	"standing",
	"walking (1)",
	"walking (2)",
	"walking (3)",
	"attacking (melee)",
	"attacking (ranged)",
	"blocking",
	"getting hit",
	"dying",
	"casting a spell",
	"changing levels",
	"quitting"
};

void ClearCursor() // CODE_FIX: this was supposed to be in cursor.cpp
{
	sgdwCursWdt = 0;
	sgdwCursWdtOld = 0;
}

/**
 * @brief Remove the cursor from the backbuffer
 */
static void scrollrt_draw_cursor_back_buffer()
{
	int i;
	BYTE *src, *dst;

	if (sgdwCursWdt == 0) {
		return;
	}

	assert(gpBuffer);
	src = sgSaveBack;
	dst = &gpBuffer[SCREENXY(sgdwCursX, sgdwCursY)];
	i = sgdwCursHgt;

	if (sgdwCursHgt != 0) {
		while (i--) {
			memcpy(dst, src, sgdwCursWdt);
			src += sgdwCursWdt;
			dst += BUFFER_WIDTH;
		}
	}

	sgdwCursXOld = sgdwCursX;
	sgdwCursYOld = sgdwCursY;
	sgdwCursWdtOld = sgdwCursWdt;
	sgdwCursHgtOld = sgdwCursHgt;
	sgdwCursWdt = 0;
}

/**
 * @brief Draw the cursor on the backbuffer
 */
static void scrollrt_draw_cursor_item()
{
	int i, mx, my, col;
	BYTE *src, *dst;

	assert(! sgdwCursWdt);

	if (pcurs <= 0 || cursW == 0 || cursH == 0) {
		return;
	}

	if (sgbControllerActive && pcurs != CURSOR_TELEPORT && !invflag && (!chrflag || plr[myplr]._pStatPts <= 0)) {
		return;
	}

	mx = MouseX - 1;
	if (mx < 0 - cursW - 1) {
		return;
	} else if (mx > SCREEN_WIDTH - 1) {
		return;
	}
	my = MouseY - 1;
	if (my < 0 - cursH - 1) {
		return;
	} else if (my > SCREEN_HEIGHT - 1) {
		return;
	}

	sgdwCursX = mx;
	sgdwCursWdt = sgdwCursX + cursW + 1;
	if (sgdwCursWdt > SCREEN_WIDTH - 1) {
		sgdwCursWdt = SCREEN_WIDTH - 1;
	}
	sgdwCursX &= ~3;
	sgdwCursWdt |= 3;
	sgdwCursWdt -= sgdwCursX;
	sgdwCursWdt++;

	sgdwCursY = my;
	sgdwCursHgt = sgdwCursY + cursH + 1;
	if (sgdwCursHgt > SCREEN_HEIGHT - 1) {
		sgdwCursHgt = SCREEN_HEIGHT - 1;
	}
	sgdwCursHgt -= sgdwCursY;
	sgdwCursHgt++;

	assert(sgdwCursWdt * sgdwCursHgt < sizeof sgSaveBack);
	assert(gpBuffer);
	dst = sgSaveBack;
	src = &gpBuffer[SCREENXY(sgdwCursX, sgdwCursY)];

	for (i = sgdwCursHgt; i != 0; i--, dst += sgdwCursWdt, src += BUFFER_WIDTH) {
		memcpy(dst, src, sgdwCursWdt);
	}

	mx++;
	my++;
	gpBufEnd = &gpBuffer[BUFFER_WIDTH * (SCREEN_HEIGHT + SCREEN_Y) - cursW - 2];

	if (pcurs >= CURSOR_FIRSTITEM) {
		col = PAL16_YELLOW + 5;
		if (plr[myplr].HoldItem._iMagical != 0) {
			col = PAL16_BLUE + 5;
		}
		if (!plr[myplr].HoldItem._iStatFlag) {
			col = PAL16_RED + 5;
		}
		CelBlitOutline(col, mx + SCREEN_X, my + cursH + SCREEN_Y - 1, pCursCels, pcurs, cursW);
		if (col != PAL16_RED + 5) {
			CelClippedDrawSafe(mx + SCREEN_X, my + cursH + SCREEN_Y - 1, pCursCels, pcurs, cursW);
		} else {
			CelDrawLightRedSafe(mx + SCREEN_X, my + cursH + SCREEN_Y - 1, pCursCels, pcurs, cursW, 1);
		}
	} else {
		CelClippedDrawSafe(mx + SCREEN_X, my + cursH + SCREEN_Y - 1, pCursCels, pcurs, cursW);
	}
}

void DrawMissilePrivate(MissileStruct *m, int sx, int sy, BOOL pre)
{
	int mx, my, nCel, frames;
	BYTE *pCelBuff;

	if (m->_miPreFlag != pre || !m->_miDrawFlag)
		return;

	pCelBuff = m->_miAnimData;
	if (!pCelBuff) {
		// app_fatal("Draw Missile 2 type %d: NULL Cel Buffer", m->_mitype);
		return;
	}
	nCel = m->_miAnimFrame;
	frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
	if (nCel < 1 || frames > 50 || nCel > frames) {
		// app_fatal("Draw Missile 2: frame %d of %d, missile type==%d", nCel, frames, m->_mitype);
		return;
	}
	mx = sx + m->_mixoff - m->_miAnimWidth2;
	my = sy + m->_miyoff;
	if (m->_miUniqTrans)
		Cl2DrawLightTbl(mx, my, m->_miAnimData, m->_miAnimFrame, m->_miAnimWidth, m->_miUniqTrans + 3);
	else if (m->_miLightFlag)
		Cl2DrawLight(mx, my, m->_miAnimData, m->_miAnimFrame, m->_miAnimWidth);
	else
		Cl2Draw(mx, my, m->_miAnimData, m->_miAnimFrame, m->_miAnimWidth);
}

/**
 * @brief Render a missile sprite
 * @param x dPiece coordinate
 * @param y dPiece coordinate
 * @param sx Backbuffer coordinate
 * @param sy Backbuffer coordinate
 * @param pre Is the sprite in the background
 */
void DrawMissile(int x, int y, int sx, int sy, BOOL pre)
{
	int i;
	MissileStruct *m;

	if (!(dFlags[x][y] & BFLAG_MISSILE))
		return;

	if (dMissile[x][y] != -1) {
		m = &missile[dMissile[x][y] - 1];
		DrawMissilePrivate(m, sx, sy, pre);
		return;
	}

	for (i = 0; i < nummissiles; i++) {
		assert(missileactive[i] < MAXMISSILES);
		m = &missile[missileactive[i]];
		if (m->_mix != x || m->_miy != y)
			continue;
		DrawMissilePrivate(m, sx, sy, pre);
	}
}

/**
 * @brief Render a monster sprite
 * @param x dPiece coordinate
 * @param y dPiece coordinate
 * @param mx Backbuffer coordinate
 * @param my Backbuffer coordinate
 */
static void DrawMonster(int x, int y, int mx, int my, int m)
{
	int nCel, frames;
	char trans;
	BYTE *pCelBuff;

	if ((DWORD)m >= MAXMONSTERS) {
		// app_fatal("Draw Monster: tried to draw illegal monster %d", m);
		return;
	}

	pCelBuff = monster[m]._mAnimData;
	if (!pCelBuff) {
		// app_fatal("Draw Monster \"%s\": NULL Cel Buffer", monster[m].mName);
		return;
	}

	nCel = monster[m]._mAnimFrame;
	frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
	if (nCel < 1 || frames > 50 || nCel > frames) {
		/*
		const char *szMode = "unknown action";
		if(monster[m]._mmode <= 17)
			szMode = szMonModeAssert[monster[m]._mmode];
		app_fatal(
			"Draw Monster \"%s\" %s: facing %d, frame %d of %d",
			monster[m].mName,
			szMode,
			monster[m]._mdir,
			nCel,
			frames);
		*/
		return;
	}

	if (!(dFlags[x][y] & BFLAG_LIT)) {
		Cl2DrawLightTbl(mx, my, monster[m]._mAnimData, monster[m]._mAnimFrame, monster[m].MType->width, 1);
	} else {
		trans = 0;
		if (monster[m]._uniqtype)
			trans = monster[m]._uniqtrans + 4;
		if (monster[m]._mmode == MM_STONE)
			trans = 2;
		if (plr[myplr]._pInfraFlag && light_table_index > 8)
			trans = 1;
		if (trans)
			Cl2DrawLightTbl(mx, my, monster[m]._mAnimData, monster[m]._mAnimFrame, monster[m].MType->width, trans);
		else
			Cl2DrawLight(mx, my, monster[m]._mAnimData, monster[m]._mAnimFrame, monster[m].MType->width);
	}
}

/**
 * @brief Render a monster sprite
 * @param pnum Player id
 * @param x dPiece coordinate
 * @param y dPiece coordinate
 * @param px Backbuffer coordinate
 * @param py Backbuffer coordinate
 * @param pCelBuff sprite buffer
 * @param nCel frame
 * @param nWidth width
 */
static void DrawPlayer(int pnum, int x, int y, int px, int py, BYTE *pCelBuff, int nCel, int nWidth)
{
	int l, frames;

	if (dFlags[x][y] & BFLAG_LIT || plr[myplr]._pInfraFlag || !setlevel && !currlevel) {
		if (!pCelBuff) {
			// app_fatal("Drawing player %d \"%s\": NULL Cel Buffer", pnum, plr[pnum]._pName);
			return;
		}
		frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
		if (nCel < 1 || frames > 50 || nCel > frames) {
			/*
			const char *szMode = "unknown action";
			if(plr[pnum]._pmode <= PM_QUIT)
				szMode = szPlrModeAssert[plr[pnum]._pmode];
			app_fatal(
				"Drawing player %d \"%s\" %s: facing %d, frame %d of %d",
				pnum,
				plr[pnum]._pName,
				szMode,
				plr[pnum]._pdir,
				nCel,
				frames);
			*/
			return;
		}
		if (pnum == pcursplr)
			Cl2DrawOutline(165, px, py, pCelBuff, nCel, nWidth);
		if (pnum == myplr) {
			Cl2Draw(px, py, pCelBuff, nCel, nWidth);
			if (plr[pnum].pManaShield)
				Cl2Draw(
				    px + plr[pnum]._pAnimWidth2 - misfiledata[MFILE_MANASHLD].mAnimWidth2[0],
				    py,
				    misfiledata[MFILE_MANASHLD].mAnimData[0],
				    1,
				    misfiledata[MFILE_MANASHLD].mAnimWidth[0]);
		} else if (!(dFlags[x][y] & BFLAG_LIT) || plr[myplr]._pInfraFlag && light_table_index > 8) {
			Cl2DrawLightTbl(px, py, pCelBuff, nCel, nWidth, 1);
			if (plr[pnum].pManaShield)
				Cl2DrawLightTbl(
				    px + plr[pnum]._pAnimWidth2 - misfiledata[MFILE_MANASHLD].mAnimWidth2[0],
				    py,
				    misfiledata[MFILE_MANASHLD].mAnimData[0],
				    1,
				    misfiledata[MFILE_MANASHLD].mAnimWidth[0],
				    1);
		} else {
			l = light_table_index;
			if (light_table_index < 5)
				light_table_index = 0;
			else
				light_table_index -= 5;
			Cl2DrawLight(px, py, pCelBuff, nCel, nWidth);
			if (plr[pnum].pManaShield)
				Cl2DrawLight(
				    px + plr[pnum]._pAnimWidth2 - misfiledata[MFILE_MANASHLD].mAnimWidth2[0],
				    py,
				    misfiledata[MFILE_MANASHLD].mAnimData[0],
				    1,
				    misfiledata[MFILE_MANASHLD].mAnimWidth[0]);
			light_table_index = l;
		}
	}
}

/**
 * @brief Render a monster sprite
 * @param x dPiece coordinate
 * @param y dPiece coordinate
 * @param sx Backbuffer coordinate
 * @param sy Backbuffer coordinate
 */
void DrawDeadPlayer(int x, int y, int sx, int sy)
{
	int i, px, py, nCel, frames;
	PlayerStruct *p;
	BYTE *pCelBuff;

	dFlags[x][y] &= ~BFLAG_DEAD_PLAYER;

	for (i = 0; i < MAX_PLRS; i++) {
		p = &plr[i];
		if (p->plractive && !p->_pHitPoints && p->plrlevel == (BYTE)currlevel && p->WorldX == x && p->WorldY == y) {
			pCelBuff = p->_pAnimData;
			if (!pCelBuff) {
				// app_fatal("Drawing dead player %d \"%s\": NULL Cel Buffer", i, p->_pName);
				break;
			}
			nCel = p->_pAnimFrame;
			frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
			if (nCel < 1 || frames > 50 || nCel > frames) {
				// app_fatal("Drawing dead player %d \"%s\": facing %d, frame %d of %d", i, p->_pName, p->_pdir, nCel, frame);
				break;
			}
			dFlags[x][y] |= BFLAG_DEAD_PLAYER;
			px = sx + p->_pxoff - p->_pAnimWidth2;
			py = sy + p->_pyoff;
			DrawPlayer(i, x, y, px, py, p->_pAnimData, p->_pAnimFrame, p->_pAnimWidth);
		}
	}
}

/**
 * @brief Render an object sprite
 * @param x dPiece coordinate
 * @param y dPiece coordinate
 * @param ox Backbuffer coordinate
 * @param oy Backbuffer coordinate
 * @param pre Is the sprite in the background
 */
static void DrawObject(int x, int y, int ox, int oy, BOOL pre)
{
	int sx, sy, xx, yy, nCel, frames;
	char bv;
	BYTE *pCelBuff;

	if (dObject[x][y] == 0 || light_table_index >= lightmax)
		return;

	if (dObject[x][y] > 0) {
		bv = dObject[x][y] - 1;
		if (object[bv]._oPreFlag != pre)
			return;
		sx = ox - object[bv]._oAnimWidth2;
		sy = oy;
	} else {
		bv = -(dObject[x][y] + 1);
		if (object[bv]._oPreFlag != pre)
			return;
		xx = object[bv]._ox - x;
		yy = object[bv]._oy - y;
		sx = (xx << 5) + ox - object[bv]._oAnimWidth2 - (yy << 5);
		sy = oy + (yy << 4) + (xx << 4);
	}

	assert((unsigned char)bv < MAXOBJECTS);

	pCelBuff = object[bv]._oAnimData;
	if (!pCelBuff) {
		// app_fatal("Draw Object type %d: NULL Cel Buffer", object[bv]._otype);
		return;
	}

	nCel = object[bv]._oAnimFrame;
	frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
	if (nCel < 1 || frames > 50 || nCel > (int)frames) {
		// app_fatal("Draw Object: frame %d of %d, object type==%d", nCel, frames, object[bv]._otype);
		return;
	}

	if (bv == pcursobj)
		CelBlitOutline(194, sx, sy, object[bv]._oAnimData, object[bv]._oAnimFrame, object[bv]._oAnimWidth);
	if (object[bv]._oLight) {
		CelClippedDrawLight(sx, sy, object[bv]._oAnimData, object[bv]._oAnimFrame, object[bv]._oAnimWidth);
	} else {
		CelClippedDraw(sx, sy, object[bv]._oAnimData, object[bv]._oAnimFrame, object[bv]._oAnimWidth);
	}
}

static void scrollrt_draw_dungeon(int sx, int sy, int dx, int dy, int eflag);

static void drawRow(int x, int y, int sx, int sy, int eflag)
{
	BYTE *dst;
	MICROS *pMap;

	level_piece_id = dPiece[x][y];
	light_table_index = dLight[x][y];

	dst = &gpBuffer[sx + sy * BUFFER_WIDTH];
	pMap = &dpiece_defs_map_2[x][y];
	cel_transparency_active = (BYTE)(nTransTable[level_piece_id] & TransList[dTransVal[x][y]]);
	for (int i = 0; i<MicroTileLen>> 1; i++) {
		arch_draw_type = i == 0 ? 1 : 0;
		level_cel_block = pMap->mt[2 * i];
		if (level_cel_block != 0) {
			drawUpperScreen(dst);
		}
		arch_draw_type = i == 0 ? 2 : 0;
		level_cel_block = pMap->mt[2 * i + 1];
		if (level_cel_block != 0) {
			drawUpperScreen(dst + 32);
		}
		dst -= BUFFER_WIDTH * 32;
	}

	scrollrt_draw_dungeon(x, y, sx, sy, eflag);
}

/**
 * This variant checks for of screen element on the lower screen
 * This function it self causes rendering issues since it will render on top of objects on the other side of walls
 * @brief Re render tile to workaround sorting issues with players walking east/west
 * @param y dPiece coordinate
 * @param x dPiece coordinate
 * @param sx Backbuffer coordinate
 * @param sy Backbuffer coordinate
 */
static void scrollrt_draw_e_flag(int x, int y, int sx, int sy)
{
	int lti_old, cta_old, lpi_old;

	lti_old = light_table_index;
	cta_old = cel_transparency_active;
	lpi_old = level_piece_id;

	drawRow(x, y, sx, sy, 0);

	light_table_index = lti_old;
	cel_transparency_active = cta_old;
	level_piece_id = lpi_old;
}

static void DrawItem(int x, int y, int sx, int sy, BOOL pre)
{
	char bItem = dItem[x][y];
	ItemStruct *pItem;

	if (bItem == 0)
		return;

	pItem = &item[bItem - 1];
	if (pItem->_iPostDraw == pre)
		return;

	assert((unsigned char)bItem <= MAXITEMS);
	int px = sx - pItem->_iAnimWidth2;
	if (bItem - 1 == pcursitem) {
		CelBlitOutline(181, px, sy, pItem->_iAnimData, pItem->_iAnimFrame, pItem->_iAnimWidth);
	}
	CelClippedDrawLight(px, sy, pItem->_iAnimData, pItem->_iAnimFrame, pItem->_iAnimWidth);
}

static void DrawMonsterHelper(int x, int y, int oy, int sx, int sy, int eflag)
{
	int mi, px, py;
	MonsterStruct *pMonster;

	if (!(dFlags[x][y] & BFLAG_LIT) && !plr[myplr]._pInfraFlag)
		return;

	mi = dMonster[x][y + oy];
	mi = mi > 0 ? mi - 1 : -(mi + 1);

	if (leveltype == DTYPE_TOWN) {
		px = sx - towner[mi]._tAnimWidth2;
		if (mi == pcursmonst) {
			CelBlitOutline(166, px, sy, towner[mi]._tAnimData, towner[mi]._tAnimFrame, towner[mi]._tAnimWidth);
		}
		assert(towner[mi]._tAnimData);
		CelClippedDraw(px, sy, towner[mi]._tAnimData, towner[mi]._tAnimFrame, towner[mi]._tAnimWidth);
		return;
	}

	if ((DWORD)mi >= MAXMONSTERS) {
		// app_fatal("Draw Monster: tried to draw illegal monster %d", mi);
	}

	pMonster = &monster[mi];
	if (pMonster->_mFlags & MFLAG_HIDDEN) {
		return;
	}

	if (pMonster->MType != NULL) {
		// app_fatal("Draw Monster \"%s\": uninitialized monster", pMonster->mName);
	}

	px = sx + pMonster->_mxoff - pMonster->MType->width2;
	py = sy + pMonster->_myoff;
	if (mi == pcursmonst) {
		Cl2DrawOutline(233, px, py, pMonster->_mAnimData, pMonster->_mAnimFrame, pMonster->MType->width);
	}
	DrawMonster(x, y, px, py, mi);
	if (eflag && !pMonster->_meflag) {
		scrollrt_draw_e_flag(x - 1, y + 1, sx - 64, sy);
	}
}

static void DrawPlayerHelper(int x, int y, int oy, int sx, int sy, int eflag)
{
	int p = dPlayer[x][y + oy];
	p = p > 0 ? p - 1 : -(p + 1);
	PlayerStruct *pPlayer = &plr[p];
	int px = sx + pPlayer->_pxoff - pPlayer->_pAnimWidth2;
	int py = sy + pPlayer->_pyoff;

	DrawPlayer(p, x, y + oy, px, py, pPlayer->_pAnimData, pPlayer->_pAnimFrame, pPlayer->_pAnimWidth);
	if (eflag && pPlayer->_peflag != 0) {
		if (pPlayer->_peflag == 2) {
			scrollrt_draw_e_flag(x - 2, y + 1, sx - 96, sy - 16);
		}
		scrollrt_draw_e_flag(x - 1, y + 1, sx - 64, sy);
	}
}

/**
 * @brief Render object sprites
 * @param sx dPiece coordinate
 * @param sy dPiece coordinate
 * @param dx Backbuffer coordinate
 * @param dy Backbuffer coordinate
 * @param eflag Should the sorting workaround be applied
 */
static void scrollrt_draw_dungeon(int sx, int sy, int dx, int dy, int eflag)
{
	int mi, px, py, nCel, nMon, negMon, frames;
	char bFlag, bDead, bObj, bItem, bPlr, bArch, bMap, negPlr, dd;
	DeadStruct *pDeadGuy;
	BYTE *pCelBuff;

	assert((DWORD)sx < MAXDUNX);
	assert((DWORD)sy < MAXDUNY);
	bFlag = dFlags[sx][sy];
	bDead = dDead[sx][sy];
	bArch = dArch[sx][sy];
	bMap = dTransVal[sx][sy];

	negMon = sy >= 1 ? dMonster[sx][sy - 1] : 0;

	if (visiondebug && bFlag & BFLAG_LIT) {
		CelClippedDraw(dx, dy, pSquareCel, 1, 64);
	}

	if (MissilePreFlag) {
		DrawMissile(sx, sy, dx, dy, TRUE);
	}

	if (light_table_index < lightmax && bDead != 0) {
		pDeadGuy = &dead[(bDead & 0x1F) - 1];
		dd = (bDead >> 5) & 7;
		px = dx - pDeadGuy->_deadWidth2;
		pCelBuff = pDeadGuy->_deadData[dd];
		assert(pDeadGuy->_deadData[dd] != NULL);
		if (pCelBuff != NULL) {
			if (pDeadGuy->_deadtrans != 0) {
				Cl2DrawLightTbl(px, dy, pCelBuff, pDeadGuy->_deadFrame, pDeadGuy->_deadWidth, pDeadGuy->_deadtrans);
			} else {
				Cl2DrawLight(px, dy, pCelBuff, pDeadGuy->_deadFrame, pDeadGuy->_deadWidth);
			}
		}
	}
	DrawObject(sx, sy, dx, dy, 1);
	DrawItem(sx, sy, dx, dy, 1);
	if (bFlag & BFLAG_PLAYERLR) {
		assert((DWORD)(sy-1) < MAXDUNY);
		DrawPlayerHelper(sx, sy, -1, dx, dy, eflag);
	}
	if (bFlag & BFLAG_MONSTLR && negMon < 0) {
		DrawMonsterHelper(sx, sy, -1, dx, dy, eflag);
	}
	if (bFlag & BFLAG_DEAD_PLAYER) {
		DrawDeadPlayer(sx, sy, dx, dy);
	}
	if (dPlayer[sx][sy] > 0) {
		DrawPlayerHelper(sx, sy, 0, dx, dy, eflag);
	}
	if (dMonster[sx][sy] > 0) {
		DrawMonsterHelper(sx, sy, 0, dx, dy, eflag);
	}
	DrawMissile(sx, sy, dx, dy, FALSE);
	DrawObject(sx, sy, dx, dy, 0);
	DrawItem(sx, sy, dx, dy, 0);

	if (bArch != 0) {
		cel_transparency_active = TransList[bMap];
		if (leveltype != DTYPE_TOWN) {
			CelClippedBlitLightTrans(&gpBuffer[dx + BUFFER_WIDTH * dy], pSpecialCels, bArch, 64);
		} else {
#if 0 // Special tree rendering, disabled in 1.09
			CelBlitFrame(&gpBuffer[dx + BUFFER_WIDTH * dy], pSpecialCels, bArch, 64);
#endif
		}
	}
}

/**
 * @brief Render a row of tile
 * @param x dPiece coordinate
 * @param y dPiece coordinate
 * @param sx Backbuffer coordinate
 * @param sy Backbuffer coordinate
 * @param chunks tile width of row
 * @param row current row being rendered
 */
static void scrollrt_draw(int x, int y, int sx, int sy, int chunks, int row)
{
	assert(gpBuffer);

	if (row & 1) {
		x -= 1;
		y += 1;
		sx -= 32;
		chunks += 1;
	}

	for (int j = 0; j < chunks; j++) {
		if (y >= 0 && y < MAXDUNY && x >= 0 && x < MAXDUNX) {
			level_piece_id = dPiece[x][y];
			if (level_piece_id != 0) {
				drawRow(x, y, sx, sy, 1);
			} else {
				world_draw_black_tile(sx, sy);
			}
		} else {
			world_draw_black_tile(sx, sy);
		}
		x++;
		y--;
		sx += 64;
	}
}

void CliDrawFlask(int col, int pct, int color)
{
	int y = 15;
    //    #########
	attrset(color | A_DIM);
	if (pct > 94)
		mvprintw(y + 0, col + 4, "#########");
	else if (pct > 89)
		mvprintw(y + 0, col + 4, "_________");
    //  ###--------##
	if (pct > 83) {
		mvprintw(y + 1, col + 2, "###");
		attroff(A_DIM);
		printw("########");
		attron(A_DIM);
		printw("##");
	} else if (pct > 78) {
		mvprintw(y + 1, col + 2, "___");
		attroff(A_DIM);
		printw("________");
		attron(A_DIM);
		printw("__");
	}
    // ###----------##
	if (pct > 72) {
		mvprintw(y + 2, col + 1, "###");
		attroff(A_DIM);
		printw("##########");
		attron(A_DIM);
		printw("##");
	} else if (pct > 67) {
		mvprintw(y + 2, col + 1, "___");
		attroff(A_DIM);
		printw("__________");
		attron(A_DIM);
		printw("__");
	}
    //###------oo----##
	if (pct > 61) {
		mvprintw(y + 3, col, "###");
		attroff(A_DIM);
		printw("######");
		attron(A_BOLD);
		printw("##");
		attroff(A_BOLD);
		printw("####");
		attron(A_DIM);
		printw("##");
	} else if (pct > 56) {
		mvprintw(y + 3, col, "___");
		attroff(A_DIM);
		printw("______");
		attron(A_BOLD);
		printw("__");
		attroff(A_BOLD);
		printw("____");
		attron(A_DIM);
		printw("__");
	}
    //###------------##
	if (pct > 50) {
		mvprintw(y + 4, col, "###");
		attroff(A_DIM);
		printw("############");
		attron(A_DIM);
		printw("##");
	} else if (pct > 44) {
		mvprintw(y + 4, col, "___");
		attroff(A_DIM);
		printw("____________");
		attron(A_DIM);
		printw("__");
	}
    //####----------###
	if (pct > 39) {
		mvprintw(y + 5, col, "####");
		attroff(A_DIM);
		printw("##########");
		attron(A_DIM);
		printw("###");
	} else if (pct > 33) {
		mvprintw(y + 5, col, "____");
		attroff(A_DIM);
		printw("__________");
		attron(A_DIM);
		printw("___");
	}
    // ####--------###
	if (pct > 28) {
		mvprintw(y + 6, col + 1, "####");
		attroff(A_DIM);
		printw("########");
		attron(A_DIM);
		printw("###");
	} else if (pct > 22) {
		mvprintw(y + 6, col + 1, "____");
		attroff(A_DIM);
		printw("________");
		attron(A_DIM);
		printw("___");
	}
    //  #############
	if (pct > 17)
		mvprintw(y + 7, col + 2, "#############");
	else if (pct > 11)
		mvprintw(y + 7, col + 2, "#############");
    //    #########
	if (pct > 6)
		mvprintw(y + 8, col + 4, "#########");
	else if (pct > 0)
		mvprintw(y + 8, col + 4, "#########");
}

void CliDoSpriteLighting(int wx, int wy) {
	if (lightmax == 3) {
		if (dLight[wx][wy] >= 2) {
			attron(A_DIM);
		}
	} else {
		if (dLight[wx][wy] >= 7) {
			attron(A_DIM);
		}
	}
}

bool CliIsDead()
{
	return plr[myplr]._pHitPoints >> 6 == 0;
}

void CliDrawDungion(int cx, int cy, int wx, int wy) {
	if ((dFlags[wx][wy] & BFLAG_EXPLORED || leveltype == DTYPE_TOWN) && !CheckNoSolid(wx, wy)) {
		if (CliIsDead()) {
			attrset(CLR_RED);
			CliDoSpriteLighting(wx, wy);
		} else {
			if (lightmax == 3) {
				switch (dLight[wx][wy]) {
				case 0:
					attrset(CLR_BW6);
					break;
				case 1:
					attrset(CLR_BW4);
					break;
				case 2:
					attrset(CLR_BW2);
					break;
				case 3:
					attrset(CLR_BW1);
				}
			} else {
				switch (dLight[wx][wy]) {
				case 0:
				case 1:
				case 2:
					attrset(CLR_BW6);
					break;
				case 3:
				case 4:
				case 5:
					attrset(CLR_BW5);
					break;
				case 6:
				case 7:
					attrset(CLR_BW4);
					break;
				case 8:
				case 9:
					attrset(CLR_BW3);
					break;
				case 10:
				case 11:
					attrset(CLR_BW2);
					break;
				case 12:
				case 13:
					attrset(CLR_BW1);
					break;
				case 14:
				case 15:
					attrset(CLR_BW1);
				}
			}
		}
		mvaddch(cy, cx, '#');
	} else {
		mvaddch(cy, cx, ' ');
	}
}

static void CliSetMonsterColor(int i)
{
	attrset(CLR_RED);
	if (CliIsDead() || !(dFlags[monster[i]._mx][monster[i]._my] & BFLAG_LIT)) {
		attrset(CLR_RED);
	} else {
		switch (monster[i].MType->mtype) {
		case MT_NZOMBIE:
		case MT_RFALLSP:
		case MT_RFALLSD:
		case MT_BSCAV:
		case MT_UNSEEN:
		case MT_NGOATMC:
		case MT_BGOATMC:
		case MT_NGOATBW:
		case MT_BGOATBW:
		case MT_NACID:
		case MT_NSNAKE:
		case MT_HORNED:
		case MT_UNRAV:
		case MT_LTCHDMN:
		case MT_SUCCUBUS:
		case MT_GARGOYLE:
			attrset(CLR_FLS);
			break;
		case MT_BZOMBIE:
		case MT_DFALLSP:
		case MT_BFALLSP:
		case MT_DFALLSD:
		case MT_BFALLSD:
		case MT_WSCAV:
		case MT_VTEXLRD:
		case MT_BMAGMA:
		case MT_FROSTC:
		case MT_BTBLACK:
		case MT_SOLBRNR:
		case MT_SNOWWICH:
		case MT_DEATHW:
			attrset(CLR_BLU);
			break;
		case MT_GZOMBIE:
		case MT_WSKELAX:
		case MT_TSKELAX:
		case MT_WSKELBW:
		case MT_TSKELBW:
		case MT_WSKELSD:
		case MT_TSKELSD:
		case MT_YSCAV:
		case MT_GGOATMC:
		case MT_GLOOM:
		case MT_GGOATBW:
		case MT_BACID:
		case MT_SKING:
		case MT_WMAGMA:
		case MT_BONEDMN:
			attrset(CLR_BW5);
			break;
		case MT_YZOMBIE:
		case MT_ILLWEAV:
		case MT_FAMILIAR:
		case MT_TOAD:
		case MT_BSNAKE:
		case MT_YMAGMA:
		case MT_MUDRUN:
		case MT_NBLACK:
		case MT_STORML:
		case MT_STORM:
		case MT_CABALIST:
		case MT_WINGED:
			attrset(CLR_YEL);
			break;
		case MT_XSKELAX:
		case MT_XSKELBW:
		case MT_XSKELSD:
		case MT_INVILORD:
		case MT_SNEAK:
		case MT_BLINK:
		case MT_RACID:
		case MT_GUARD:
		case MT_GSNAKE:
		case MT_OBLORD:
		case MT_RBLACK:
		case MT_MAEL:
		case MT_GOLEM:
		case MT_MAGISTR:
		case MT_ADVOCATE:
			attrset(CLR_BW2);
			break;
		}
	}
}

static void CliDrawMonsterHelper(int x, int y, int ox, int oy)
{
	int mi, px, py;
	MonsterStruct *pMonster;

	if (!(dFlags[x][y] & BFLAG_LIT) && !plr[myplr]._pInfraFlag)
		return;

	mi = dMonster[x][y];
	mi = mi > 0 ? mi - 1 : -(mi + 1);

	if (leveltype == DTYPE_TOWN) {
		attrset(CLR_GRN);
		mvaddch(towner[mi]._ty - oy, towner[mi]._tx - ox, *towner[mi]._tName);
		return;
	}

	pMonster = &monster[mi];
	if (pMonster->_mFlags & MFLAG_HIDDEN) {
		return;
	}

	CliSetMonsterColor(mi);
	CliDoSpriteLighting(pMonster->_mx, pMonster->_my);
	mvaddch(pMonster->_my - oy, pMonster->_mx - ox, *pMonster->mName);
}

static void CliDrawPlayerHelper(int x, int y, int ox, int oy)
{
	int p = dPlayer[x][y];
	p = p > 0 ? p - 1 : -(p + 1);
	PlayerStruct *pPlayer = &plr[p];

	if (dFlags[x][y] & BFLAG_LIT || plr[myplr]._pInfraFlag || !setlevel && !currlevel) {
		attrset(CLR_GRN);
		if (CliIsDead())
			attrset(CLR_RED);
		CliDoSpriteLighting(x, y);
		if (p == myplr)
			mvaddch(pPlayer->_py - oy, pPlayer->_px - ox, '@');
		else
			mvaddch(pPlayer->_py - oy, pPlayer->_px - ox, *pPlayer->_pName);
	}
}

static void CliDrawObject(int ox, int oy, int x, int y, bool pre)
{
	int oi, xx, yy;

	if (dObject[x][y] == 0 || dLight[x][y] >= lightmax)
		return;

	if (dObject[x][y] > 0) {
		oi = dObject[x][y] - 1;
		if (object[oi]._oPreFlag != pre)
			return;
	} else {
		oi = -(dObject[x][y] + 1);
		if (object[oi]._oPreFlag != pre)
			return;
		x = object[oi]._ox - x;
		y = object[oi]._oy - y;
	}

	assert((unsigned char)oi < MAXOBJECTS);

	if (object[oi]._oSelFlag)
		attrset(CLR_YEL);
	else
		attrset(CLR_FLS);

	char t = '#';
	switch (object[oi]._otype) {
	case OBJ_L1LIGHT:
	case OBJ_CANDLE1:
	case OBJ_CANDLE2:
	case OBJ_CANDLEO:
	case OBJ_TORCHL:
	case OBJ_TORCHR:
	case OBJ_TORCHL2:
	case OBJ_TORCHR2:
		t = 'i';
		break;
	case OBJ_LEVER:
	case OBJ_FLAMELVR:
	case OBJ_SWITCHSKL:
		t = 'l';
		break;
	case OBJ_CHEST1:
	case OBJ_TCHEST1:
	case OBJ_BOOK2L:
	case OBJ_BOOK2R:
	case OBJ_CHEST2:
	case OBJ_TCHEST2:
	case OBJ_CHEST3:
	case OBJ_TCHEST3:
	case OBJ_SIGNCHEST:
	case OBJ_BOOKSHELF:
	case OBJ_BARREL:
	case OBJ_BARRELEX:
	case OBJ_SKELBOOK:
	case OBJ_BOOKCASEL:
	case OBJ_BOOKCASER:
	case OBJ_BOOKSTAND:
	case OBJ_BLINDBOOK:
	case OBJ_BLOODBOOK:
	case OBJ_PEDISTAL:
	case OBJ_STEELTOME:
	case OBJ_STORYBOOK:
	case OBJ_SLAINHERO:
		t = '0';
		break;
	case OBJ_L1LDOOR:
	case OBJ_L2LDOOR:
	case OBJ_L3LDOOR:
		attrset(CLR_FLS);
		if (object[oi]._oVar4 == 0)
			t = '|';
		else
			t = '/';
		break;
	case OBJ_L1RDOOR:
	case OBJ_L2RDOOR:
	case OBJ_L3RDOOR:
		attrset(CLR_FLS);
		if (object[oi]._oVar4 == 0)
			t = '-';
		else
			t = '\\';
		break;
	case OBJ_CRUX1:
	case OBJ_CRUX2:
	case OBJ_CRUX3:
			t = 't';
		break;
	case OBJ_SHRINEL:
	case OBJ_SHRINER:
	case OBJ_BLOODFTN:
	case OBJ_PURIFYINGFTN:
	case OBJ_GOATSHRINE:
	case OBJ_CAULDRON:
	case OBJ_MURKYFTN:
	case OBJ_TEARFTN:
			t = '?';
		break;
	case OBJ_ARMORSTAND:
	case OBJ_WARARMOR:
	case OBJ_WARWEAP:
	case OBJ_WEAPONRACK:
	case OBJ_LAZSTAND:
			t = 'w';
		break;
	case OBJ_MUSHPATCH:
			t = 'm';
		break;
	}

	if (CliIsDead())
		attrset(CLR_RED);
	if (object[oi]._oLight)
		CliDoSpriteLighting(x, y);
	mvaddch(oy, ox, t);
}

static void CliDrawItem(int ox, int oy, int x, int y, bool pre)
{
	char bItem = dItem[x][y];
	ItemStruct *pItem;

	if (bItem == 0)
		return;

	pItem = &item[bItem - 1];
	if (pItem->_iPostDraw == pre)
		return;

	attrset(CLR_BLU);
	if (bItem - 1 == pcursitem) {
		attron(A_BOLD);
	}
	if (CliIsDead())
		attrset(CLR_RED);
	CliDoSpriteLighting(x, y);

	char t = '#';
	switch (pItem->_itype) {
	case ITYPE_SWORD:
		t = 'i';
		break;
	case ITYPE_AXE:
		t = 'p';
		break;
	case ITYPE_BOW:
		t = 'b';
		break;
	case ITYPE_MACE:
		t = 't';
		break;
	case ITYPE_SHIELD:
		t = '0';
		break;
	case ITYPE_HELM:
		t = 'n';
		break;
	case ITYPE_STAFF:
		t = 'l';
		break;
	case ITYPE_GOLD:
		t = '$';
		break;
	case ITYPE_RING:
		t = 'o';
		break;
	case ITYPE_AMULET:
		t = 'y';
		break;
	}
	mvaddch(oy, ox, t);
}

bool CliFindTrigger(int x, int y)
{
	for (int i = 0; i < numtrigs; i++) {
		if (trigs[i]._tx == x && trigs[i]._ty == y)
			return true;
	}

	for (int i = 0; i < MAXQUESTS; i++) {
		if (i == QTYPE_VB || currlevel != quests[i]._qlevel || quests[i]._qslvl == 0)
			continue;
		if (quests[i]._qtx == x && quests[i]._qty == y)
			return true;
	}

	return false;
}

/**
 * @brief Configure render and process screen rows
 * @param x Center of view in dPiece coordinate
 * @param y Center of view in dPiece coordinate
 */
static void CliDrawGame()
{
	int x = plr[myplr]._px;
	int y = plr[myplr]._py;
	int width = 80;
	int sx = 0;
	if (invflag || sbookflag) {
		width -= 40;
	}
	if (chrflag || questlog) {
		width -= 40;
		sx += 40;
	}

	for (int cy = 0; cy < 17; cy++) {
		for (int cx = 0; cx < width; cx++) {
			int ox = x - width / 2;
			int oy = y - 17 / 2;
			int wx = cx + ox;
			int wy = cy + oy;
			if (wx >= 0 && wx < MAXDUNX && wy >= 0 && wy < MAXDUNX && (dFlags[wx][wy] & BFLAG_EXPLORED || leveltype == DTYPE_TOWN)) {
				attrset(CLR_BW0);
				CliDrawDungion(cx + sx, cy, wx, wy);
				CliDrawObject(cx + sx, cy, wx, wy, true);
				CliDrawItem(cx + sx, cy, wx, wy, true);
				if (dPlayer[wx][wy] != 0) {
					CliDrawPlayerHelper(wx, wy, ox - sx, oy);
				} else if (dMonster[wx][wy] != 0) {
					CliDrawMonsterHelper(wx, wy, ox - sx, oy);
				} else if (dFlags[wx][wy] & BFLAG_MISSILE) {
					attrset(CLR_YEL);
					if (CliIsDead())
						attrset(CLR_RED);
					CliDoSpriteLighting(wx, wy);
					mvaddch(cy, cx + sx, '*');
				} else if (CliFindTrigger(wx, wy)) {
					attrset(CLR_YEL);
					mvaddch(cy, cx + sx, '#');
				}
				CliDrawObject(cx + sx, cy, wx, wy, false);
				CliDrawItem(cx + sx, cy, wx, wy, false);
			} else {
				attrset(CLR_BW0);
				mvaddch(cy, cx + sx, ' ');
			}
		}
	}
}

void CliDrawFrame(int x, int y, int w, int h)
{
	attrset(CLR_FLS);
	mvaddch(y, x, '+');
	mvaddch(y, x + w - 1, '+');
	mvaddch(y + h - 1, x, '+');
	mvaddch(y + h - 1, x + w - 1, '+');
	mvhline(y, x + 1, '-', w - 2);
	mvhline(y + h - 1, x + 1, '-', w - 2);
	mvvline(y + 1, x, '|', h - 2);
	mvvline(y + 1, x + w - 1, '|', h - 2);
}

void CliDrawBox(int color, int x, int y, int w, int h, char c = '#')
{
	attrset(color);
	for (int cy = y; cy < y + h; cy++)
		mvhline(cy, x, c, w);
}

static void CliDrawPanel()
{
	attrset(CLR_FLS | A_DIM | A_BOLD);
	mvhline(17, 0, '_', 80);

	attrset(CLR_FLS | A_DIM | A_BOLD);
	if (setlevel)
		mvprintw(17, 27, quest_level_names[(BYTE)setlvlnum]);
	else if (currlevel) {
		char desc[256];
		sprintf(desc, "Level: %i", currlevel);
		mvprintw(17, 27, desc);
	}

	// Clear panel
	CliDrawBox(CLR_BW5, 0, 18, 80, 6);

	for (int i = 0; i < MAXBELTITEMS; i++) {
		if (plr[myplr].SpdList[i]._itype == ITYPE_NONE) {
			attrset(CLR_BW2);
			mvaddch(18, 29 + (3 * i), '#');
			continue;
		}
		attrset(CLR_GRN);
		if (pcursinvitem == i + INVITEM_BELT_FIRST)
			attron(A_BOLD);
		switch (AllItemsList[plr[myplr].SpdList[i].IDidx].iMiscId) {
			case IMISC_HEAL:
				attron(CLR_RED);
				mvaddch(18, 29 + (3 * i), 'o');
				break;
			case IMISC_FULLHEAL:
				attron(CLR_RED);
				mvaddch(18, 29 + (3 * i), 'O');
				break;
			case IMISC_SCROLL:
				attron(CLR_FLS);
				mvaddch(18, 29 + (3 * i), 'I');
				break;
			case IMISC_MANA:
				attron(CLR_BLU);
				mvaddch(18, 29 + (3 * i), 'o');
				break;
			case IMISC_FULLMANA:
				attron(CLR_BLU);
				mvaddch(18, 29 + (3 * i), 'O');
				break;
			case IMISC_REJUV:
				attron(CLR_YEL);
				mvaddch(18, 29 + (3 * i), 'o');
				break;
			case IMISC_FULLREJUV:
				attron(CLR_YEL);
				mvaddch(18, 29 + (3 * i), 'O');
				break;
			default:
				mvaddch(18, 29 + (3 * i), '#');
				break;
		}
	}

	attrset(CLR_BW6 | A_UNDERLINE | A_BOLD | A_REVERSE);
	if (plr[myplr]._pStatPts)
		attron(CLR_RED);
	mvprintw(18, 1, "C");
	attroff(A_UNDERLINE | A_BOLD);
	printw("har");
	attron(CLR_BW6);
	attron(A_UNDERLINE | A_BOLD);
	mvprintw(19, 1, "Q");
	attroff(A_UNDERLINE | A_BOLD);
	printw("uest");
	attron(A_UNDERLINE | A_BOLD);
	mvprintw(21, 1, "M");
	attroff(A_UNDERLINE | A_BOLD);
	printw("enu");
	attroff(CLR_BW6);

	CliDrawFlask(8, 100 * plr[myplr]._pHitPoints / plr[myplr]._pMaxHP, CLR_RED);
	CliDrawBox(CLR_BW0, 26, 19, 28, 5, ' ');
	CliDrawFlask(55, 100 * plr[myplr]._pMana / plr[myplr]._pMaxMana, CLR_BLU);

	attrset(CLR_BW6 | A_UNDERLINE | A_BOLD | A_REVERSE);
	mvprintw(18, 73, "I");
	attroff(A_UNDERLINE | A_BOLD);
	printw("nv");
	attron(A_UNDERLINE | A_BOLD);
	mvprintw(19, 73, "S");
	attroff(A_UNDERLINE | A_BOLD);
	printw("pells");
	attroff(CLR_BW6);
}

/**
 * @brief Configure render and process screen rows
 * @param x Center of view in dPiece coordinate
 * @param y Center of view in dPiece coordinate
 */
static void DrawGame(int x, int y)
{
	int i, sx, sy, chunks, blocks;
	int wdt, nSrcOff, nDstOff;

	sx = (SCREEN_WIDTH % 64) / 2;
	sy = (VIEWPORT_HEIGHT % 32) / 2;

	if (zoomflag) {
		chunks = ceil(SCREEN_WIDTH / 64);
		blocks = ceil(VIEWPORT_HEIGHT / 32);

		gpBufStart = &gpBuffer[BUFFER_WIDTH * SCREEN_Y];
		gpBufEnd = &gpBuffer[BUFFER_WIDTH * (VIEWPORT_HEIGHT + SCREEN_Y)];
	} else {
		sy -= 32;

		chunks = ceil(SCREEN_WIDTH / 2 / 64) + 1; // TODO why +1?
		blocks = ceil(VIEWPORT_HEIGHT / 2 / 32);

		gpBufStart = &gpBuffer[(-17 + SCREEN_Y) * BUFFER_WIDTH];
		gpBufEnd = &gpBuffer[(160 + SCREEN_Y) * BUFFER_WIDTH];
	}

	sx += ScrollInfo._sxoff + SCREEN_X;
	sy += ScrollInfo._syoff + SCREEN_Y + 15;

	// Center screen
	x -= chunks;
	y--;

	// Keep evaulating untill MicroTiles can't affect screen
	blocks += ceil(MicroTileLen / 2);

	if (PANELS_COVER) {
		if (zoomflag) {
			if (chrflag || questlog) {
				x += 2;
				y -= 2;
				sx += 288;
				chunks -= 4;
			}
			if (invflag || sbookflag) {
				x += 2;
				y -= 2;
				sx -= 32;
				chunks -= 4;
			}
		}
	}

	switch (ScrollInfo._sdir) {
	case SDIR_NONE:
		break;
	case SDIR_N:
		sy -= 32;
		x--;
		y--;
		blocks++;
		break;
	case SDIR_NE:
		sy -= 32;
		x--;
		y--;
		chunks++;
		blocks++;
		break;
	case SDIR_E:
		chunks++;
		break;
	case SDIR_SE:
		chunks++;
		blocks++;
		break;
	case SDIR_S:
		blocks++;
		break;
	case SDIR_SW:
		sx -= 64;
		x--;
		y++;
		chunks++;
		blocks++;
		break;
	case SDIR_W:
		sx -= 64;
		x--;
		y++;
		chunks++;
		break;
	case SDIR_NW:
		sx -= 64;
		sy -= 32;
		x -= 2;
		chunks++;
		blocks++;
		break;
	}

	for (i = 0; i < (blocks << 1); i++) {
		scrollrt_draw(x, y, sx, sy, chunks, i);
		sy += 16;
		if (i & 1)
			y++;
		else
			x++;
	}
	gpBufStart = &gpBuffer[BUFFER_WIDTH * SCREEN_Y];
	gpBufEnd = &gpBuffer[BUFFER_WIDTH * (SCREEN_HEIGHT + SCREEN_Y)];

	if (zoomflag)
		return;

	nSrcOff = SCREENXY(32, VIEWPORT_HEIGHT / 2 - 17);
	nDstOff = SCREENXY(0, VIEWPORT_HEIGHT - 2);
	wdt = SCREEN_WIDTH / 2;
	if (PANELS_COVER) {
		if (chrflag || questlog) {
			nSrcOff = SCREENXY(112, VIEWPORT_HEIGHT / 2 - 17);
			nDstOff = SCREENXY(SPANEL_WIDTH, VIEWPORT_HEIGHT - 2);
			wdt = (SCREEN_WIDTH - SPANEL_WIDTH) / 2;
		} else if (invflag || sbookflag) {
			nSrcOff = SCREENXY(112, VIEWPORT_HEIGHT / 2 - 17);
			nDstOff = SCREENXY(0, VIEWPORT_HEIGHT - 2);
			wdt = (SCREEN_WIDTH - SPANEL_WIDTH) / 2;
		}
	}

	int hgt;
	BYTE *src, *dst1, *dst2;

	src = &gpBuffer[nSrcOff];
	dst1 = &gpBuffer[nDstOff];
	dst2 = &gpBuffer[nDstOff + BUFFER_WIDTH];

	for (hgt = VIEWPORT_HEIGHT / 2; hgt != 0; hgt--, src -= BUFFER_WIDTH + wdt, dst1 -= 2 * (BUFFER_WIDTH + wdt), dst2 -= 2 * (BUFFER_WIDTH + wdt)) {
		for (i = wdt; i != 0; i--) {
			*dst1++ = *src;
			*dst1++ = *src;
			*dst2++ = *src;
			*dst2++ = *src;
			src++;
		}
	}
}

int CliItemColor(ItemStruct *i, int id)
{
	int color = CLR_BW1;
	if (i->_itype != ITYPE_NONE) {
		color = CLR_BW5;
		if (i->_iMagical != ITEM_QUALITY_NORMAL)
			color = CLR_BLU;
		if (!i->_iStatFlag)
			color = CLR_RED;
		if (pcursinvitem == id) {
			color |= A_BOLD;
		}
	}

	return color;
}

void CliDrawInv()
{
	int x = 40;
	int color;
	CliDrawFrame(x, 0, 40, 17);
	CliDrawBox(CLR_BW4, x + 1, 1, 38, 7);

	// check which inventory rectangle the mouse is in, if any
	int slot = 25;
	for (int r = 0; (DWORD)r < NUM_XY_SLOTS; r++) {
		if (MouseX >= InvRect[r].X && MouseX < InvRect[r].X + (INV_SLOT_SIZE_PX + 1) && MouseY >= InvRect[r].Y - (INV_SLOT_SIZE_PX + 1) && MouseY < InvRect[r].Y) {
			slot = r;
			break;
		}
	}
	char tmpstring[32];
	sprintf(tmpstring, "slot %d", slot);
	mvprintw(2, 2, tmpstring);

	color = CliItemColor(&plr[myplr].InvBody[INVLOC_HAND_LEFT], INVLOC_HAND_LEFT);
	CliDrawBox(color, x + 2, 1, 4, 6); // right hand
	color = CliItemColor(&plr[myplr].InvBody[INVLOC_RING_LEFT], INVLOC_RING_LEFT);
	CliDrawBox(color, x + 7, 5, 2, 2); // right ring


	color = CliItemColor(&plr[myplr].InvBody[INVLOC_HEAD], INVLOC_HEAD);
	CliDrawBox(color, x + 14, 1, 4, 4); // head
	color = CliItemColor(&plr[myplr].InvBody[INVLOC_CHEST], INVLOC_CHEST);
	CliDrawBox(color, x + 19, 1, 4, 6); // torso
	color = CliItemColor(&plr[myplr].InvBody[INVLOC_AMULET], INVLOC_AMULET);
	CliDrawBox(color, x + 24, 1, 2, 2); // amulet

	color = CliItemColor(&plr[myplr].InvBody[INVLOC_RING_RIGHT], INVLOC_RING_RIGHT);
	CliDrawBox(color, x + 31, 5, 2, 2); // left ring
	if (plr[myplr].InvBody[INVLOC_HAND_LEFT]._itype != ITYPE_NONE && plr[myplr].InvBody[INVLOC_HAND_LEFT]._iLoc == ILOC_TWOHAND)
		color = CliItemColor(&plr[myplr].InvBody[INVLOC_HAND_LEFT], INVLOC_HAND_LEFT) | A_DIM;
	else
		color = CliItemColor(&plr[myplr].InvBody[INVLOC_HAND_RIGHT], INVLOC_HAND_RIGHT);
	CliDrawBox(color, x + 34, 1, 4, 6); // left hand

	attrset(CLR_FLS);
	mvhline(7, x + 1, '-', 40 - 2);
	CliDrawBox(CLR_BW4, x + 1, 8, 38, 8);
	CliDrawBox(CLR_BW1, x + 10, 8, 20, 8);

	if (slot >= 25) {
		CliDrawBox(CLR_BW2, x + 10 + ((slot - 25) % 10 * 2), 8 + ((slot - 25) / 10 * 2), 2, 2);
	}

	for (int j = 0; j < NUM_INV_GRID_ELEM; j++) {
		if (plr[myplr].InvGrid[j] > 0) {
			int ii = plr[myplr].InvGrid[j] - 1;
			color = CliItemColor(&plr[myplr].InvList[ii], ii + INVITEM_INV_FIRST);
			int ci = plr[myplr].InvList[ii]._iCurs + CURSOR_FIRSTITEM;
			int iw = InvItemWidth[ci] / 28 * 2;
			int ih = InvItemHeight[ci] / 28 * 2;
			CliDrawBox(color, x + 10 + (j % 10 * 2), 8 + (j / 10 * 2) - ih + 2, iw, ih);
		}
	}
}

void CliDrawSpellBook()
{
	int x = 40;
	CliDrawFrame(40, 0, 40, 17);
	CliDrawBox(CLR_BW5, x + 1, 1, 38, 15);
}

void CliPaddedPrint(int x, int y, int padding, int n)
{
	char tmpstring[40];
	sprintf(tmpstring, "%*d", padding, n);
	mvprintw(y, x, tmpstring);
}

void CliStatColor(int current, int base)
{
	if (current > base)
		attrset(CLR_BLU);
	else if (current < base)
		attrset(CLR_RED);
	else
		attrset(CLR_BW6);
}

void CliDrawChr()
{
	CliDrawFrame(0, 0, 40, 17);
	CliDrawBox(CLR_BW0, 1, 1, 38, 15);
	attrset(CLR_BW6);
	mvprintw(1, 2, plr[myplr]._pName);
	mvprintw(1, 17, ClassStrTbl[plr[myplr]._pClass]);

	mvprintw(2, 2, "Level");
	CliPaddedPrint(8, 2, 2, plr[myplr]._pLevel);

	mvprintw(2, 17, "Experience");
	CliPaddedPrint(28, 2, 10, plr[myplr]._pExperience);

	mvprintw(3, 17, "Next level");
	if (plr[myplr]._pLevel == MAXCHARLEVEL - 1) {
		attrset(CLR_FLS);
		mvprintw(3, 31, "None");
	} else {
		CliPaddedPrint(28, 3, 10, plr[myplr]._pNextExper);
	}

	attrset(CLR_BW6);
	mvprintw(5, 17, "Gold");
	CliPaddedPrint(32, 5, 6, plr[myplr]._pGold);

	mvprintw(5, 6, "Base");
	mvprintw(5, 11, "Now");

	mvprintw(6, 2, "Str");
	if (MaxStats[plr[myplr]._pClass][ATTRIB_STR] == plr[myplr]._pBaseStr)
		attrset(CLR_FLS);
	CliPaddedPrint(7, 6, 3, plr[myplr]._pBaseStr);
	CliStatColor(plr[myplr]._pStrength, plr[myplr]._pBaseStr);
	CliPaddedPrint(11, 6, 3, plr[myplr]._pStrength);

	attrset(CLR_BW6);
	mvprintw(7, 2, "Mag");
	if (MaxStats[plr[myplr]._pClass][ATTRIB_MAG] == plr[myplr]._pBaseMag)
		attrset(CLR_FLS);
	CliPaddedPrint(7, 7, 3, plr[myplr]._pBaseMag);
	CliStatColor(plr[myplr]._pMagic, plr[myplr]._pBaseMag);
	CliPaddedPrint(11, 7, 3, plr[myplr]._pMagic);

	attrset(CLR_BW6);
	mvprintw(8, 2, "Dex");
	if (MaxStats[plr[myplr]._pClass][ATTRIB_DEX] == plr[myplr]._pBaseDex)
		attrset(CLR_FLS);
	CliPaddedPrint(7, 8, 3, plr[myplr]._pBaseDex);
	CliStatColor(plr[myplr]._pDexterity, plr[myplr]._pBaseDex);
	CliPaddedPrint(11, 8, 3, plr[myplr]._pDexterity);

	attrset(CLR_BW6);
	mvprintw(9, 2, "Vit");
	if (MaxStats[plr[myplr]._pClass][ATTRIB_VIT] == plr[myplr]._pBaseVit)
		attrset(CLR_FLS);
	CliPaddedPrint(7, 9, 3, plr[myplr]._pBaseVit);
	CliStatColor(plr[myplr]._pVitality, plr[myplr]._pBaseVit);
	CliPaddedPrint(11, 9, 3, plr[myplr]._pVitality);

	attrset(CLR_BW6);
	mvprintw(10, 2, "Pts");
	if (plr[myplr]._pStatPts) {
		CliPaddedPrint(7, 10, 3, plr[myplr]._pStatPts);

		int slot = 0;
		for (int i = 0; i < 4; i++) {
			if (MouseX >= ChrBtnsRect[i].x
				&& MouseX <= ChrBtnsRect[i].x + ChrBtnsRect[i].w
				&& MouseY >= ChrBtnsRect[i].y
				&& MouseY <= ChrBtnsRect[i].h + ChrBtnsRect[i].y) {
				slot = i;
				break;
			}
		}
		attrset(CLR_BN1);
		mvprintw(6 + slot, 11, " + ");
	}

	attrset(CLR_BW6);
	mvprintw(12, 2, "Life");
	CliStatColor(plr[myplr]._pMaxHP, plr[myplr]._pMaxHPBase);
	CliPaddedPrint(7, 12, 3, plr[myplr]._pMaxHP >> 6);
	if (plr[myplr]._pHitPoints < plr[myplr]._pMaxHP)
		attrset(CLR_RED);
	CliPaddedPrint(11, 12, 3, plr[myplr]._pHitPoints >> 6);

	attrset(CLR_BW6);
	mvprintw(13, 2, "Mana");
	CliStatColor(plr[myplr]._pMaxMana, plr[myplr]._pMaxManaBase);
	CliPaddedPrint(7, 13, 3, plr[myplr]._pMaxMana >> 6);
	if (plr[myplr]._pMana < plr[myplr]._pMaxMana)
		attrset(CLR_RED);
	CliPaddedPrint(11, 13, 3, plr[myplr]._pMana >> 6);

	attrset(CLR_BW6);
	mvprintw(7, 17, "AC");
	CliStatColor(plr[myplr]._pIBonusAC, 0);
	CliPaddedPrint(35, 7, 3, plr[myplr]._pIBonusAC + plr[myplr]._pIAC + plr[myplr]._pDexterity / 5);

	attrset(CLR_BW6);
	mvprintw(8, 17, "To hit");
	CliStatColor(plr[myplr]._pIBonusToHit, 0);
	CliPaddedPrint(34, 8, 3, (plr[myplr]._pDexterity >> 1) + plr[myplr]._pIBonusToHit + 50);
	mvprintw(8, 37, "%%");

	attrset(CLR_BW6);
	mvprintw(9, 17, "Dam");
	CliStatColor(plr[myplr]._pIBonusDam, 0);
	int mindam = plr[myplr]._pIMinDam;
	mindam += plr[myplr]._pIBonusDam * mindam / 100;
	mindam += plr[myplr]._pIBonusDamMod;
	if (plr[myplr].InvBody[INVLOC_HAND_LEFT]._itype == ITYPE_BOW) {
		if (plr[myplr]._pClass == PC_ROGUE)
			mindam += plr[myplr]._pDamageMod;
		else
			mindam += plr[myplr]._pDamageMod >> 1;
	} else {
		mindam += plr[myplr]._pDamageMod;
	}
	int maxdam = plr[myplr]._pIMaxDam;
	maxdam += plr[myplr]._pIBonusDam * maxdam / 100;
	maxdam += plr[myplr]._pIBonusDamMod;
	if (plr[myplr].InvBody[INVLOC_HAND_LEFT]._itype == ITYPE_BOW) {
		if (plr[myplr]._pClass == PC_ROGUE)
			maxdam += plr[myplr]._pDamageMod;
		else
			maxdam += plr[myplr]._pDamageMod >> 1;
	} else {
		maxdam += plr[myplr]._pDamageMod;
	}
	char chrstr[40];
	char chrstr2[40];
	sprintf(chrstr, "%i-%i", mindam, maxdam);
	sprintf(chrstr2, "%*s", 9, chrstr);
	mvprintw(9, 29, chrstr2);

	attrset(CLR_BW6);
	mvprintw(11, 17, "Resist magic");
	if (plr[myplr]._pMagResist < 75) {
		if (plr[myplr]._pMagResist)
			attrset(CLR_BLU);
		CliPaddedPrint(35, 11, 2, plr[myplr]._pMagResist);
		mvprintw(11, 37, "%%");
	} else {
		attrset(CLR_FLS);
		mvprintw(11, 35, "MAX");
	}

	attrset(CLR_BW6);
	mvprintw(12, 17, "Resist fire");
	if (plr[myplr]._pFireResist < 75) {
		if (plr[myplr]._pFireResist)
			attrset(CLR_BLU);
		CliPaddedPrint(35, 12, 2, plr[myplr]._pFireResist);
		mvprintw(12, 37, "%%");
	} else {
		attrset(CLR_FLS);
		mvprintw(12, 35, "MAX");
	}

	attrset(CLR_BW6);
	mvprintw(13, 17, "Resist lightning");
	if (plr[myplr]._pLghtResist < 75) {
		if (plr[myplr]._pLghtResist)
			attrset(CLR_BLU);
		CliPaddedPrint(35, 13, 2, plr[myplr]._pLghtResist);
		mvprintw(13, 37, "%%");
	} else {
		attrset(CLR_FLS);
		mvprintw(13, 35, "MAX");
	}
}

void CliDrawQuestLog()
{
	CliDrawFrame(0, 0, 40, 17);
	CliDrawBox(CLR_BW5, 1, 1, 38, 15);
}

/**
 * @brief Start rendering of screen, town variation
 * @param StartX Center of view in dPiece coordinate
 * @param StartY Center of view in dPiece coordinate
 */
void DrawView(int StartX, int StartY)
{
	DrawGame(StartX, StartY);
	CliDrawGame();
	if (automapflag) {
		DrawAutomap();
	}

	if (stextflag && !qtextflag)
		DrawSText();
	if (invflag) {
		DrawInv();
		CliDrawInv();
	} else if (sbookflag) {
		DrawSpellBook();
		CliDrawSpellBook();
	}

	DrawDurIcon();

	if (chrflag) {
		DrawChr();
		CliDrawChr();
	} else if (questlog) {
		DrawQuestLog();
		CliDrawQuestLog();
	}

	CliDrawPanel();

	if (!chrflag && plr[myplr]._pStatPts != 0 && !spselflag
		&& (!questlog || SCREEN_HEIGHT >= SPANEL_HEIGHT + PANEL_HEIGHT + 74 || SCREEN_WIDTH >= 4 * SPANEL_WIDTH)) {
		DrawLevelUpIcon();
	}
	if (uitemflag) {
		DrawUniqueInfo();
	}
	if (qtextflag) {
		DrawQText();
	}
	if (spselflag) {
		DrawSpellList();
	}
	if (dropGoldFlag) {
		DrawGoldSplit(dropGoldValue);
	}
	if (helpflag) {
		DrawHelp();
	}
	if (msgflag) {
		DrawDiabloMsg();
	}
	if (deathflag) {
		RedBack();
	} else if (PauseMode != 0) {
		gmenu_draw_pause();
	}

	DrawPlrMsg();
	gmenu_draw();
	doom_draw();
	DrawInfoBox();
	DrawLifeFlask();
	DrawManaFlask();
}

/**
 * @brief Render the whole screen black
 */
void ClearScreenBuffer()
{
	lock_buf(3);

	assert(gpBuffer);

	int i;
	BYTE *dst;

	dst = &gpBuffer[SCREENXY(0, 0)];

	for (i = 0; i < SCREEN_HEIGHT; i++, dst += BUFFER_WIDTH) {
		memset(dst, 0, SCREEN_WIDTH);
	}

	unlock_buf(3);
}

#ifdef _DEBUG
/**
 * @brief Scroll the screen when mouse is close to the edge
 */
void ScrollView()
{
	BOOL scroll;

	if (pcurs >= CURSOR_FIRSTITEM)
		return;

	scroll = FALSE;

	if (MouseX < 20) {
		if (dmaxy - 1 <= ViewY || dminx >= ViewX) {
			if (dmaxy - 1 > ViewY) {
				ViewY++;
				scroll = TRUE;
			}
			if (dminx < ViewX) {
				ViewX--;
				scroll = TRUE;
			}
		} else {
			ViewY++;
			ViewX--;
			scroll = TRUE;
		}
	}
	if (MouseX > SCREEN_WIDTH - 20) {
		if (dmaxx - 1 <= ViewX || dminy >= ViewY) {
			if (dmaxx - 1 > ViewX) {
				ViewX++;
				scroll = TRUE;
			}
			if (dminy < ViewY) {
				ViewY--;
				scroll = TRUE;
			}
		} else {
			ViewY--;
			ViewX++;
			scroll = TRUE;
		}
	}
	if (MouseY < 20) {
		if (dminy >= ViewY || dminx >= ViewX) {
			if (dminy < ViewY) {
				ViewY--;
				scroll = TRUE;
			}
			if (dminx < ViewX) {
				ViewX--;
				scroll = TRUE;
			}
		} else {
			ViewX--;
			ViewY--;
			scroll = TRUE;
		}
	}
	if (MouseY > SCREEN_HEIGHT - 20) {
		if (dmaxy - 1 <= ViewY || dmaxx - 1 <= ViewX) {
			if (dmaxy - 1 > ViewY) {
				ViewY++;
				scroll = TRUE;
			}
			if (dmaxx - 1 > ViewX) {
				ViewX++;
				scroll = TRUE;
			}
		} else {
			ViewX++;
			ViewY++;
			scroll = TRUE;
		}
	}

	if (scroll)
		ScrollInfo._sdir = SDIR_NONE;
}
#endif

/**
 * @brief Initialize the FPS meter
 */
void EnableFrameCount()
{
	frameflag = frameflag == 0;
	framestart = GetTickCount();
}

/**
 * @brief Display the current average FPS over 1 sec
 */
static void DrawFPS()
{
	DWORD tc, frames;
	char String[12];
	HDC hdc;

	if (frameflag && gbActive && pPanelText) {
		frameend++;
		tc = GetTickCount();
		frames = tc - framestart;
		if (tc - framestart >= 1000) {
			framestart = tc;
			framerate = 1000 * frameend / frames;
			frameend = 0;
		}
		wsprintf(String, "%d FPS", framerate);
		PrintGameStr(8, 65, String, COL_RED);
	}
}

/**
 * @brief Update part of the screen from the backbuffer
 * @param dwX Backbuffer coordinate
 * @param dwY Backbuffer coordinate
 * @param dwWdt Backbuffer coordinate
 * @param dwHgt Backbuffer coordinate
 */
static void DoBlitScreen(DWORD dwX, DWORD dwY, DWORD dwWdt, DWORD dwHgt)
{
	RECT SrcRect;

	SrcRect.left = dwX + SCREEN_X;
	SrcRect.top = dwY + SCREEN_Y;
	SrcRect.right = SrcRect.left + dwWdt - 1;
	SrcRect.bottom = SrcRect.top + dwHgt - 1;

	BltFast(dwX, dwY, &SrcRect);
}

/**
 * @brief Check render pipline and blit indivudal screen parts
 * @param dwHgt Section of screen to update from top to bottom
 * @param draw_desc Render info box
 * @param draw_hp Render halth bar
 * @param draw_mana Render mana bar
 * @param draw_sbar Render belt
 * @param draw_btn Render panel buttons
 */
static void DrawMain(int dwHgt, BOOL draw_desc, BOOL draw_hp, BOOL draw_mana, BOOL draw_sbar, BOOL draw_btn)
{
	int ysize;
	DWORD dwTicks;
	BOOL retry;

	ysize = dwHgt;

	if (!gbActive) {
		return;
	}

	assert(ysize >= 0 && ysize <= SCREEN_HEIGHT);

	if (ysize > 0) {
		DoBlitScreen(0, 0, SCREEN_WIDTH, ysize);
	}
	if (ysize < SCREEN_HEIGHT) {
		if (draw_sbar) {
			DoBlitScreen(PANEL_LEFT + 204, PANEL_TOP + 5, 232, 28);
		}
		if (draw_desc) {
			DoBlitScreen(PANEL_LEFT + 176, PANEL_TOP + 46, 288, 60);
		}
		if (draw_mana) {
			DoBlitScreen(PANEL_LEFT + 460, PANEL_TOP, 88, 72);
			DoBlitScreen(PANEL_LEFT + 564, PANEL_TOP + 64, 56, 56);
		}
		if (draw_hp) {
			DoBlitScreen(PANEL_LEFT + 96, PANEL_TOP, 88, 72);
		}
		if (draw_btn) {
			DoBlitScreen(PANEL_LEFT + 8, PANEL_TOP + 5, 72, 119);
			DoBlitScreen(PANEL_LEFT + 556, PANEL_TOP + 5, 72, 48);
			if (gbMaxPlayers > 1) {
				DoBlitScreen(PANEL_LEFT + 84, PANEL_TOP + 91, 36, 32);
				DoBlitScreen(PANEL_LEFT + 524, PANEL_TOP + 91, 36, 32);
			}
		}
		if (sgdwCursWdtOld != 0) {
			DoBlitScreen(sgdwCursXOld, sgdwCursYOld, sgdwCursWdtOld, sgdwCursHgtOld);
		}
		if (sgdwCursWdt != 0) {
			DoBlitScreen(sgdwCursX, sgdwCursY, sgdwCursWdt, sgdwCursHgt);
		}
	}
}

void scrollrt_draw_game_screen(BOOL draw_cursor)
{
	int hgt;

	if (force_redraw == 255) {
		force_redraw = 0;
		hgt = SCREEN_HEIGHT;
	} else {
		hgt = 0;
	}

	if (draw_cursor) {
		lock_buf(0);
		scrollrt_draw_cursor_item();
		unlock_buf(0);
	}

	DrawMain(hgt, 0, 0, 0, 0, 0);

	if (draw_cursor) {
		lock_buf(0);
		scrollrt_draw_cursor_back_buffer();
		unlock_buf(0);
	}
	RenderPresent();
}

/**
 * @brief Render the game
 */
void DrawAndBlit()
{
	int hgt;
	BOOL ddsdesc, ctrlPan;

	if (!gbRunGame) {
		return;
	}

	if (SCREEN_WIDTH > PANEL_WIDTH || SCREEN_HEIGHT > VIEWPORT_HEIGHT + PANEL_HEIGHT || force_redraw == 255) {
		drawhpflag = TRUE;
		drawmanaflag = TRUE;
		drawbtnflag = TRUE;
		drawsbarflag = TRUE;
		ddsdesc = FALSE;
		ctrlPan = TRUE;
		hgt = SCREEN_HEIGHT;
	} else {
		ddsdesc = TRUE;
		ctrlPan = FALSE;
		hgt = VIEWPORT_HEIGHT;
	}

	force_redraw = 0;

	lock_buf(0);
	DrawView(ViewX, ViewY);
	if (ctrlPan) {
		ClearCtrlPan();
	}
	if (drawhpflag) {
		UpdateLifeFlask();
	}
	if (drawmanaflag) {
		UpdateManaFlask();
	}
	if (drawbtnflag) {
		DrawCtrlPan();
	}
	if (drawsbarflag) {
		DrawInvBelt();
	}
	if (talkflag) {
		DrawTalkPan();
		hgt = SCREEN_HEIGHT;
	}
	scrollrt_draw_cursor_item();

	DrawFPS();

	unlock_buf(0);

	DrawMain(hgt, ddsdesc, drawhpflag, drawmanaflag, drawsbarflag, drawbtnflag);

	lock_buf(0);
	scrollrt_draw_cursor_back_buffer();
	unlock_buf(0);
	RenderPresent();

	drawhpflag = FALSE;
	drawmanaflag = FALSE;
	drawbtnflag = FALSE;
	drawsbarflag = FALSE;
}

DEVILUTION_END_NAMESPACE
