#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <mips.h>
#include <n64.h>
#include "gfx.h"
#include "gu.h"
#include "menu.h"
#include "resource.h"
#include "z64.h"
#include "zu.h"

enum state
{
  STATE_LOAD,
  STATE_RENDER,
  STATE_UNLOAD,
};

struct item_data
{
  enum state      state;
  int             scene_index;
  int             scene_next;
  int             room_index;
  int             room_next;
  int             no_rooms;
  void           *scene_file;
  void           *room_file;
  struct zu_mesh  room_mesh;
  float           scale;
  float           x;
  float           y;
  float           z;
  float           yaw;
};

static void execute_scene_config(int config_index, Gfx **p_opa, Gfx **p_xlu)
{
  static int buf = 0;
  static Gfx opa_buf[2][128];
  static Gfx xlu_buf[2][128];
  Gfx *opa = opa_buf[buf];
  Gfx *opa_p = opa;
  Gfx *opa_e = opa + 128;
  Gfx *xlu = xlu_buf[buf];
  Gfx *xlu_p = xlu;
  Gfx *xlu_e = xlu + 128;
  buf = (buf + 1) % 2;
  /* save graphics context */
  Gfx *z_opa_p = z64_ctxt.gfx->poly_opa_disp_p;
  Gfx *z_opa_e = z64_ctxt.gfx->poly_opa_disp_e;
  Gfx *z_xlu_p = z64_ctxt.gfx->poly_xlu_disp_p;
  Gfx *z_xlu_e = z64_ctxt.gfx->poly_xlu_disp_e;
  /* inject context variables */
  z64_ctxt.gfx->poly_opa_disp_p = opa_p;
  z64_ctxt.gfx->poly_opa_disp_e = opa_e;
  z64_ctxt.gfx->poly_xlu_disp_p = xlu_p;
  z64_ctxt.gfx->poly_xlu_disp_e = xlu_e;
  /* execute config */
  z64_scene_config_table[config_index](&z64_game);
  /* retrieve updated pointers */
  opa_p = z64_ctxt.gfx->poly_opa_disp_p;
  xlu_p = z64_ctxt.gfx->poly_xlu_disp_p;
  /* restore graphics context */
  z64_ctxt.gfx->poly_opa_disp_p = z_opa_p;
  z64_ctxt.gfx->poly_opa_disp_e = z_opa_e;
  z64_ctxt.gfx->poly_xlu_disp_p = z_xlu_p;
  z64_ctxt.gfx->poly_xlu_disp_e = z_xlu_e;
  /* end dlist buffers and return pointers */
  gSPEndDisplayList(opa_p);
  gSPEndDisplayList(xlu_p);
  *p_opa = opa;
  *p_xlu = xlu;
}

static void draw_crosshair(struct menu_item *item)
{
  struct item_data *data = item->data;
  struct gfx_texture *texture = resource_get(RES_TEXTURE_CROSSHAIR);
  /* define meshes */
  static Vtx lat_mesh[] =
  {
    gdSPDefVtx(-16, 0, 16,  0,  0),
    gdSPDefVtx(16,  0, 16,  62, 0),
    gdSPDefVtx(-16, 0, -16, 0,  62),
    gdSPDefVtx(16,  0, -16, 62, 62),
  };
  static Vtx vert_mesh[] =
  {
    gdSPDefVtx(-16, 16,  0, 0,  0),
    gdSPDefVtx(16,  16,  0, 62, 0),
    gdSPDefVtx(-16, -16, 0, 0,  62),
    gdSPDefVtx(16,  -16, 0, 62, 62),
  };
  /* create modelview matrices */
  float xscale = 1.5f;
  float yscale = 1.5f;
  float zscale = 1.5f;
  Mtx m;
  MtxF mf;
  MtxF mt;
  {
    guTranslateF(&mf, data->x, data->y, data->z);
  }
  {
    guScaleF(&mt, xscale, yscale, zscale);
    guMtxCatF(&mt, &mf, &mf);
  }
  guMtxF2L(&mf, &m);
  Mtx *p_latz_mtx = gfx_data_append(&m, sizeof(m));
  {
    guRotateF(&mt, -M_PI / 2.f, 0.f, 1.f, 0.f);
    guMtxCatF(&mt, &mf, &mf);
  }
  guMtxF2L(&mf, &m);
  Mtx *p_latx_mtx = gfx_data_append(&m, sizeof(m));
  {
    guTranslateF(&mf, data->x, data->y, data->z);
  }
  {
    guRotateF(&mt, data->yaw, 0.f, 1.f, 0.f);
    guMtxCatF(&mt, &mf, &mf);
  }
  {
    guScaleF(&mt, xscale, yscale, zscale);
    guMtxCatF(&mt, &mf, &mf);
  }
  guMtxF2L(&mf, &m);
  Mtx *p_vert_mtx = gfx_data_append(&m, sizeof(m));
  /* build dlist */
  gfx_disp
  (
    gsDPPipeSync(),
    /* rsp state */
    gsSPGeometryMode(~0, G_ZBUFFER),
    /* rdp state */
    gsDPSetCycleType(G_CYC_1CYCLE),
    /* texture engine */
    gsSPTexture(0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON),
    gsDPSetTexturePersp(G_TP_PERSP),
    gsDPSetTextureDetail(G_TD_CLAMP),
    gsDPSetTextureLUT(G_TT_NONE),
    /* texture filter */
    gsDPSetTextureFilter(G_TF_BILERP),
    gsDPSetTextureConvert(G_TC_FILT),
    /* color combiner */
    gsDPSetCombineMode(G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM),
    /* blender */
    gsDPSetAlphaCompare(G_AC_NONE),
    gsDPSetDepthSource(G_ZS_PIXEL),
    gsDPSetRenderMode(G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2),
    /* memory interface */
    gsDPSetColorDither(G_CD_DISABLE),
    gsDPSetAlphaDither(G_AD_DISABLE),
    /* load meshes */
    gsSPMatrix(p_latx_mtx, G_MTX_MODELVIEW | G_MTX_LOAD),
    gsSPVertex(&lat_mesh, 4, 0),
    gsSPMatrix(p_vert_mtx, G_MTX_MODELVIEW | G_MTX_LOAD),
    gsSPVertex(&vert_mesh, 4, 4),
    gsSPMatrix(p_latz_mtx, G_MTX_MODELVIEW | G_MTX_LOAD),
    gsSPVertex(&lat_mesh, 4, 8),
  );
  /* render navigation indicator primitives */
  gfx_disp(gsDPSetPrimColor(0, 0, 0xFF, 0xFF, 0xFF, 0x40));
  if (z64_input_direct.pad & BUTTON_Z) {
    gfx_rdp_load_tile(texture, 2);
    gfx_disp(gsSP2Triangles(4, 5, 6, 0, 6, 5, 7, 0));
  }
  else {
    gfx_rdp_load_tile(texture, 1);
    gfx_disp(gsSP2Triangles(0, 1, 2, 0, 2, 1, 3, 0));
  }
  /* render crosshair primitives */
  gfx_rdp_load_tile(texture, 0);
  gfx_disp
  (
    gsDPSetPrimColor(0, 0, 0x00, 0x00, 0xFF, 0x40),
    gsSP2Triangles(8, 9, 10, 0, 10, 9, 11, 0),
    gsDPPipeSync(),
    gsDPSetPrimColor(0, 0, 0xFF, 0x00, 0x00, 0x40),
    gsSP2Triangles(0, 1, 2, 0, 2, 1, 3, 0),
    gsDPPipeSync(),
    gsDPSetPrimColor(0, 0, 0x00, 0xFF, 0x00, 0x40),
    gsSP2Triangles(4, 5, 6, 0, 6, 5, 7, 0),
  );
}

static int think_proc(struct menu_item *item)
{
  struct item_data *data = item->data;
  /* handle scene changes */
  if (data->scene_index != z64_game.scene_index) {
    data->scene_next = z64_game.scene_index;
    data->room_next = z64_game.room_index;
  }
  if ((data->scene_index != data->scene_next ||
       data->room_index != data->room_next) &&
      data->state == STATE_RENDER)
    data->state = STATE_UNLOAD;
  return 0;
}

static int draw_proc(struct menu_item *item,
                     struct menu_draw_params *draw_params)
{
  static Gfx null_dl = gsSPEndDisplayList();
  struct item_data *data = item->data;
  /* handle input */
  uint16_t pad = z64_input_direct.pad;
  if (!(pad & BUTTON_R)) {
    if (pad & BUTTON_Z) {
      if (pad & BUTTON_D_UP)
        data->y += 50.f;
      if (pad & BUTTON_D_DOWN)
        data->y += -50.f;
      if (pad & BUTTON_D_LEFT) {
        data->x -= cos(data->yaw) * 50.f;
        data->z += sin(data->yaw) * 50.f;
      }
      if (pad & BUTTON_D_RIGHT) {
        data->x -= cos(data->yaw) * -50.f;
        data->z += sin(data->yaw) * -50.f;
      }
    }
    else {
      if (pad & BUTTON_D_UP) {
        data->x -= sin(data->yaw) * 50.f;
        data->z -= cos(data->yaw) * 50.f;
      }
      if (pad & BUTTON_D_DOWN) {
        data->x -= sin(data->yaw) * -50.f;
        data->z -= cos(data->yaw) * -50.f;
      }
      if (pad & BUTTON_D_LEFT)
        data->yaw += .2f;
      if (pad & BUTTON_D_RIGHT)
        data->yaw -= .2f;
    }
  }
  /* load resources */
  if (data->state == STATE_LOAD) {
    /* initialize segment table */
    z64_stab_t stab = z64_stab;
    /* load scene */
    if (data->scene_index != data->scene_next || !data->scene_file) {
      if (data->scene_file)
        free(data->scene_file);
      data->scene_index = data->scene_next;
      data->room_index = -1;
      z64_scene_table_t *scene_entry = &z64_scene_table[data->scene_index];
      uint32_t scene_vrom_start = scene_entry->scene_vrom_start;
      uint32_t scene_vrom_end = scene_entry->scene_vrom_end;
      uint32_t scene_vrom_size = scene_vrom_end - scene_vrom_start;
      data->scene_file = malloc(scene_vrom_size);
      zu_getfile(scene_vrom_start, data->scene_file, scene_vrom_size);
    }
    stab.seg[Z64_SEG_SCENE] = MIPS_KSEG0_TO_PHYS(data->scene_file);
    /* populate room list */
    struct zu_file room_files[0x100];
    zu_scene_rooms(zu_sr_header(data->scene_file,
                                z64_file.scene_setup_index, &stab),
                   room_files, 0x100, &data->no_rooms, &stab);
    /* load room */
    if (data->room_index != data->room_next || !data->room_file) {
      if (data->room_file) {
        free(data->room_file);
        zu_mesh_destroy(&data->room_mesh);
      }
      data->room_index = data->room_next;
      uint32_t room_vrom_start = room_files[data->room_index].vrom_start;
      uint32_t room_vrom_end = room_files[data->room_index].vrom_end;
      uint32_t room_vrom_size = room_vrom_end - room_vrom_start;
      data->room_file = malloc(room_vrom_size);
      zu_getfile(room_vrom_start, data->room_file, room_vrom_size);
      stab.seg[Z64_SEG_ROOM] = MIPS_KSEG0_TO_PHYS(data->room_file);
      /* populate mesh */
      zu_room_mesh(zu_sr_header(data->room_file,
                                z64_file.scene_setup_index, &stab),
                   &data->room_mesh, &stab);
      /* populate vertex list */
      struct zu_vlist vlist;
      zu_vlist_init(&vlist);
      stab.seg[0x08] = MIPS_KSEG0_TO_PHYS(&null_dl);
      stab.seg[0x09] = MIPS_KSEG0_TO_PHYS(&null_dl);
      stab.seg[0x0A] = MIPS_KSEG0_TO_PHYS(&null_dl);
      stab.seg[0x0B] = MIPS_KSEG0_TO_PHYS(&null_dl);
      stab.seg[0x0C] = MIPS_KSEG0_TO_PHYS(&null_dl);
      stab.seg[0x0D] = MIPS_KSEG0_TO_PHYS(&null_dl);
      for (int i = 0; i < ZU_MESH_TYPES; ++i)
        for (int j = 0; j < data->room_mesh.all[i].size; ++j)
          zu_vlist_add_dl(&vlist, &stab,
                          zu_seg_locate(&stab,
                                        data->room_mesh.all[i].dlists[j]));
      /* compute bounding box */
      struct zu_bbox bbox;
      zu_vlist_bbox(&vlist, &bbox);
      /* set orientation */
      {
        data->x = (bbox.x1 + bbox.x2) / 2.f;
        data->y = (bbox.y1 + bbox.y2) / 2.f;
        data->z = (bbox.z1 + bbox.z2) / 2.f;
        data->yaw = 0.f;
      }
      zu_vlist_destroy(&vlist);
    }
    /* proceed to rendering */
    data->state = STATE_RENDER;
  }
  /* render room */
  if (data->state == STATE_RENDER && data->room_file) {
    /* initialize rcp for rendering rooms */
    static void *zbuf = NULL;
    if (!zbuf)
      zbuf = memalign(64, 2 * Z64_SCREEN_WIDTH * Z64_SCREEN_HEIGHT);
    gfx_disp
    (
      /* clear z buffer */
      gsDPPipeSync(),
      gsDPSetCycleType(G_CYC_FILL),
      gsDPSetRenderMode(G_RM_NOOP, G_RM_NOOP2),
      gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, Z64_SCREEN_WIDTH, zbuf),
      gsDPSetFillColor((GPACK_ZDZ(G_MAXFBZ, 0) << 16) |
                       GPACK_ZDZ(G_MAXFBZ, 0)),
      gsDPFillRectangle(0, 0, Z64_SCREEN_WIDTH - 1, Z64_SCREEN_HEIGHT - 1),
      gsDPPipeSync(),
      gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, Z64_SCREEN_WIDTH,
                        ZU_MAKE_SEG(Z64_SEG_CIMG, 0)),
      gsDPSetDepthImage(zbuf),
      /* rsp settings */
      gsSPSegment(Z64_SEG_SCENE, data->scene_file),
      gsSPSegment(Z64_SEG_ROOM, data->room_file),
      gsSPSegment(0x08, &null_dl),
      gsSPSegment(0x09, &null_dl),
      gsSPSegment(0x0A, &null_dl),
      gsSPSegment(0x0B, &null_dl),
      gsSPSegment(0x0C, &null_dl),
      gsSPSegment(0x0D, &null_dl),
      gsSPGeometryMode(~0,
                       G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING |
                       G_SHADING_SMOOTH),
      /* rdp settings */
      gsDPSetAlphaCompare(G_AC_NONE),
      gsDPSetDepthSource(G_ZS_PIXEL),
      gsDPSetAlphaDither(G_AD_DISABLE),
      gsDPSetColorDither(G_CD_DISABLE),
      gsDPSetCombineKey(G_OFF),
      gsDPSetTextureConvert(G_TC_FILT),
      gsDPSetTextureFilter(G_TF_BILERP),
      gsDPSetTextureLOD(G_TL_TILE),
      gsDPSetTexturePersp(G_TP_PERSP),
      gsDPSetCycleType(G_CYC_2CYCLE),
      gsDPPipelineMode(G_PM_NPRIMITIVE),
      gsDPSetEnvColor(0xFF, 0xFF, 0xFF, 0xFF),
      gsDPSetFogColor(0x00, 0x00, 0x00, 0x00),
      gsDPSetScissor(G_SC_NON_INTERLACE, 32, 32,
                     Z64_SCREEN_WIDTH - 32, Z64_SCREEN_HEIGHT - 32),
    );
    /* create projection matrix */
    {
      Mtx m;
      MtxF mf;
      MtxF mt;
      guPerspectiveF(&mf, NULL, atanf(2.f),
                     (float)Z64_SCREEN_WIDTH / (float)Z64_SCREEN_HEIGHT,
                     50.f, 5000.f, 1.f);
      {
        guScaleF(&mt, data->scale, data->scale, data->scale);
        guMtxCatF(&mt, &mf, &mf);
      }
      {
        guRotateF(&mt, M_PI / 6.f, 1.f, 0.f, 0.f);
        guMtxCatF(&mt, &mf, &mf);
      }
      {
        guTranslateF(&mt, 0.f, -100.f, -200.f);
        guMtxCatF(&mt, &mf, &mf);
      }
      {
        guRotateF(&mt, -data->yaw, 0.f, 1.f, 0.f);
        guMtxCatF(&mt, &mf, &mf);
      }
      {
        guTranslateF(&mt, -data->x, -data->y, -data->z);
        guMtxCatF(&mt, &mf, &mf);
      }
      guMtxF2L(&mf, &m);
      gfx_disp(gsSPMatrix(gfx_data_append(&m, sizeof(m)),
                          G_MTX_PROJECTION | G_MTX_LOAD));
    }
    /* create modelview matrix */
    {
      Mtx m;
      guMtxIdent(&m);
      gfx_disp(gsSPMatrix(gfx_data_append(&m, sizeof(m)),
                          G_MTX_MODELVIEW | G_MTX_LOAD));
    }
    /* configure lights */
    static Lights3 lights = gdSPDefLights3(0x40, 0x40, 0x40,
                                           0xD0, 0xD0, 0xD0, 127, 0, 0,
                                           0xA0, 0xA0, 0xA0, 0, 127, 0,
                                           0x70, 0x70, 0x70, -127, 0, 0);
    gfx_disp(gsSPSetLights3(lights));
    /* create scene config dlists */
    Gfx *opa;
    Gfx *xlu;
    execute_scene_config(z64_scene_table[z64_game.scene_index].scene_config,
                         &opa, &xlu);
    /* render */
    for (int i = 0; i < ZU_MESH_TYPES; ++i) {
      if ((data->room_mesh.near.size > 0 && i == ZU_MESH_NEAR) ||
          (data->room_mesh.opa.size > 0 && i == ZU_MESH_OPA))
        gfx_disp(gsSPDisplayList(opa));
      if ((data->room_mesh.far.size > 0 && i == ZU_MESH_FAR) ||
          (data->room_mesh.xlu.size > 0 && i == ZU_MESH_XLU))
        gfx_disp(gsSPDisplayList(xlu));
      for (int j = 0; j < data->room_mesh.all[i].size; ++j)
        gfx_disp(gsSPDisplayList(data->room_mesh.all[i].dlists[j]));
    }
    draw_crosshair(item);
    /* restore rcp modes */
    gfx_mode_init();
    /* draw info */
    gfx_mode_set(GFX_MODE_COLOR, GPACK_RGBA8888(0xC0, 0xC0, 0xC0,
                                                draw_params->alpha));
    gfx_printf(draw_params->font, 36, 44, "scene %i", data->scene_index);
    gfx_printf(draw_params->font, 36, 44 + menu_get_cell_height(item->owner, 1),
               "room  %i", data->room_index);
  }
  /* wait for rendering to finish before unloading */
  if (data->state == STATE_UNLOAD)
    data->state = STATE_LOAD;
  return 1;
}

static int navigate_proc(struct menu_item *item, enum menu_navigation nav)
{
  struct item_data *data = item->data;
  if (data->state == STATE_RENDER && (z64_input_direct.pad & BUTTON_R)) {
    if (nav == MENU_NAVIGATE_UP)
      data->room_next = (data->room_next + 1) % data->no_rooms;
    else if (nav == MENU_NAVIGATE_DOWN)
      data->room_next = (data->room_next + data->no_rooms - 1) % data->no_rooms;
  }
  return 1;
}

static int activate_proc(struct menu_item *item)
{
  struct item_data *data = item->data;
  if (data->room_index != z64_game.room_index) {
    z64_LoadRoom(&z64_game, &z64_game.room_index, data->room_index);
    z64_UnloadRoom(&z64_game, &z64_game.room_index);
  }
  z64_xyz_t pos = {data->x, data->y, data->z};
  z64_link.common.pos_1 = z64_link.common.pos_2 = pos;
  z64_link.common.rot_2.y = z64_link.target_yaw = 0x8000 + data->yaw *
                                                  0x8000 / M_PI;
  z64_link.drop_y = pos.y;
  z64_link.drop_distance = 0;
  menu_return_top(item->owner);
  return 1;
}

void explorer_create(struct menu *menu)
{
  menu_init(menu, MENU_NOVALUE, MENU_NOVALUE, MENU_NOVALUE);
  struct menu_item *item = menu_item_add(menu, 0, 0, NULL, 0);
  menu->selector = item;
  struct item_data *data = malloc(sizeof(*data));
  data->state = STATE_LOAD;
  data->scene_index = -1;
  data->scene_file = NULL;
  data->room_index = -1;
  data->room_file = NULL;
  data->scale = .5f;
  item->data = data;
  item->think_proc = think_proc;
  item->draw_proc = draw_proc;
  item->navigate_proc = navigate_proc;
  item->activate_proc = activate_proc;
}
