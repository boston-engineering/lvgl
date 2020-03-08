/**
 * @file lv_win.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_win.h"
#if LV_USE_WIN != 0

#include "../lv_core/lv_debug.h"
#include "../lv_themes/lv_theme.h"
#include "../lv_core/lv_disp.h"

/*********************
 *      DEFINES
 *********************/
#define LV_OBJX_NAME "lv_win"
#define DEF_TITLE "Window"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_res_t lv_win_signal(lv_obj_t * win, lv_signal_t sign, void * param);
static lv_design_res_t lv_win_header_design(lv_obj_t * header, const lv_area_t * clip_area, lv_design_mode_t mode);
static lv_style_list_t * lv_win_get_style(lv_obj_t * win, uint8_t part);
static void lv_win_realign(lv_obj_t * win);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_design_cb_t ancestor_header_design;
static lv_signal_cb_t ancestor_signal;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Create a window objects
 * @param par pointer to an object, it will be the parent of the new window
 * @param copy pointer to a window object, if not NULL then the new object will be copied from it
 * @return pointer to the created window
 */
lv_obj_t * lv_win_create(lv_obj_t * par, const lv_obj_t * copy)
{
    LV_LOG_TRACE("window create started");

    /*Create the ancestor object*/
    lv_obj_t * new_win = lv_obj_create(par, copy);
    LV_ASSERT_MEM(new_win);
    if(new_win == NULL) return NULL;

    if(ancestor_signal == NULL) ancestor_signal = lv_obj_get_signal_cb(new_win);

    /*Allocate the object type specific extended data*/
    lv_win_ext_t * ext = lv_obj_allocate_ext_attr(new_win, sizeof(lv_win_ext_t));
    LV_ASSERT_MEM(ext);
    if(ext == NULL) {
        lv_obj_del(new_win);
        return NULL;
    }

    ext->page          = NULL;
    ext->header        = NULL;
    ext->title_txt    = lv_mem_alloc(strlen(DEF_TITLE) + 1);
    strcpy(ext->title_txt, DEF_TITLE);

    /*Init the new window object*/
    if(copy == NULL) {
        /* Set a size which fits into the parent.
         * Don't use `par` directly because if the window is created on a page it is moved to the
         * scrollable so the parent has changed */
        lv_coord_t w;
        lv_coord_t h;
        if(par) {
            w = lv_obj_get_width_fit(lv_obj_get_parent(new_win));
            h = lv_obj_get_height_fit(lv_obj_get_parent(new_win));
        }
        else {
            w = lv_disp_get_hor_res(NULL);
            h = lv_disp_get_ver_res(NULL);
        }

        lv_obj_set_size(new_win, w, h);

        ext->page = lv_page_create(new_win, NULL);
        lv_obj_add_protect(ext->page, LV_PROTECT_PARENT);
        lv_page_set_scrlbar_mode(ext->page, LV_SCRLBAR_MODE_AUTO);
        lv_obj_clean_style_list(ext->page, LV_PAGE_PART_BG);

        /*Create a holder for the header*/
        ext->header = lv_obj_create(new_win, NULL);
        /*Move back to window background because it's automatically moved to the content page*/
        lv_obj_add_protect(ext->header, LV_PROTECT_PARENT);
        lv_obj_set_parent(ext->header, new_win);
        if(ancestor_header_design == NULL) ancestor_header_design = lv_obj_get_design_cb(ext->header);
        lv_obj_set_height(ext->header, LV_DPI / 2);

        lv_obj_set_design_cb(ext->header, lv_win_header_design);
        lv_obj_set_signal_cb(new_win, lv_win_signal);

        lv_theme_apply(new_win, LV_THEME_WIN);
    }
    /*Copy an existing object*/
    else {
        lv_win_ext_t * copy_ext = lv_obj_get_ext_attr(copy);
        /*Create the objects*/
        ext->header   = lv_obj_create(new_win, copy_ext->header);
        ext->title_txt    = lv_mem_alloc(strlen(copy_ext->title_txt) + 1);
        strcpy(ext->title_txt, copy_ext->title_txt);
        ext->page     = lv_page_create(new_win, copy_ext->page);

        /*Copy the buttons*/
        lv_obj_t * child;
        child = lv_obj_get_child_back(copy_ext->header, NULL);
        child = lv_obj_get_child_back(copy_ext->header, child); /*Sip the title*/
        while(child != NULL) {
            lv_obj_t * btn = lv_btn_create(ext->header, child);
            lv_img_create(btn, lv_obj_get_child(child, NULL));
            child = lv_obj_get_child_back(copy_ext->header, child);
        }

        lv_obj_set_signal_cb(new_win, lv_win_signal);
    }

    /*Refresh the style with new signal function*/
    lv_obj_refresh_style(new_win);

    lv_win_realign(new_win);

    LV_LOG_INFO("window created");

    return new_win;
}

/**
 * Delete all children of the scrl object, without deleting scrl child.
 * @param win pointer to an object
 */
void lv_win_clean(lv_obj_t * win)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_obj_t * scrl = lv_page_get_scrl(win);
    lv_obj_clean(scrl);
}

/*======================
 * Add/remove functions
 *=====================*/

/**
 * Add control button to the header of the window
 * @param win pointer to a window object
 * @param img_src an image source ('lv_img_t' variable, path to file or a symbol)
 * @return pointer to the created button object
 */
lv_obj_t * lv_win_add_btn(lv_obj_t * win, const void * img_src)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);
    LV_ASSERT_NULL(img_src);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);

    lv_obj_t * btn = lv_btn_create(ext->header, NULL);
    lv_theme_apply(btn, LV_THEME_WIN_BTN);
    lv_coord_t btn_size = lv_obj_get_height_fit(ext->header);
    lv_obj_set_size(btn, btn_size, btn_size);

    lv_obj_t * img = lv_img_create(btn, NULL);
    lv_obj_set_click(img, false);
    lv_img_set_src(img, img_src);

    lv_win_realign(win);

    return btn;
}

/*=====================
 * Setter functions
 *====================*/

/**
 * Can be assigned to a window control button to close the window
 * @param btn pointer to the control button on teh widows header
 * @param evet the event type
 */
void lv_win_close_event_cb(lv_obj_t * btn, lv_event_t event)
{
    LV_ASSERT_OBJ(btn, "lv_btn");

    if(event == LV_EVENT_RELEASED) {
        lv_obj_t * win = lv_win_get_from_btn(btn);

        lv_obj_del(win);
    }
}

/**
 * Set the title of a window
 * @param win pointer to a window object
 * @param title string of the new title
 */
void lv_win_set_title(lv_obj_t * win, const char * title)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);
    LV_ASSERT_STR(title);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);

    ext->title_txt    = lv_mem_realloc(ext->title_txt, strlen(title) + 1);
    strcpy(ext->title_txt, title);
    lv_obj_invalidate(ext->header);
}

/**
 * Set the height of the header
 * @param win pointer to a window object
 * @param height height of the header
 */
void lv_win_set_header_height(lv_obj_t * win, lv_coord_t height)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);

    lv_obj_set_height(ext->header, height);
    lv_win_realign(win);
}

/**
 * Set the size of the content area.
 * @param win pointer to a window object
 * @param w width
 * @param h height (the window will be higher with the height of the header)
 */
void lv_win_set_content_size(lv_obj_t * win, lv_coord_t w, lv_coord_t h)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    h += lv_obj_get_height(ext->header);

    lv_obj_set_size(win, w, h);
}

/**
 * Set the layout of the window
 * @param win pointer to a window object
 * @param layout the layout from 'lv_layout_t'
 */
void lv_win_set_layout(lv_obj_t * win, lv_layout_t layout)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    lv_page_set_scrl_layout(ext->page, layout);
}

/**
 * Set the scroll bar mode of a window
 * @param win pointer to a window object
 * @param sb_mode the new scroll bar mode from  'lv_sb_mode_t'
 */
void lv_win_set_sb_mode(lv_obj_t * win, lv_scrlbar_mode_t sb_mode)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    lv_page_set_scrlbar_mode(ext->page, sb_mode);
}
/**
 * Set focus animation duration on `lv_win_focus()`
 * @param win pointer to a window object
 * @param anim_time duration of animation [ms]
 */
void lv_win_set_anim_time(lv_obj_t * win, uint16_t anim_time)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_page_set_anim_time(lv_win_get_content(win), anim_time);
}

/**
 * Set drag status of a window. If set to 'true' window can be dragged like on a PC.
 * @param win pointer to a window object
 * @param en whether dragging is enabled
 */
void lv_win_set_drag(lv_obj_t * win, bool en)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext    = lv_obj_get_ext_attr(win);
    lv_obj_t * win_header = ext->header;
    lv_obj_set_drag_parent(win_header, en);
    lv_obj_set_drag(win, en);
}

/*=====================
 * Getter functions
 *====================*/

/**
 * Get the title of a window
 * @param win pointer to a window object
 * @return title string of the window
 */
const char * lv_win_get_title(const lv_obj_t * win)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    return ext->title_txt;
}

/**
 * Get the content holder object of window (`lv_page`) to allow additional customization
 * @param win pointer to a window object
 * @return the Page object where the window's content is
 */
lv_obj_t * lv_win_get_content(const lv_obj_t * win)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    return ext->page;
}

/**
 * Get the header height
 * @param win pointer to a window object
 * @return header height
 */
lv_coord_t lv_win_get_header_height(const lv_obj_t * win)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    return lv_obj_get_height(ext->header);
}

/**
 * Get the pointer of a widow from one of  its control button.
 * It is useful in the action of the control buttons where only button is known.
 * @param ctrl_btn pointer to a control button of a window
 * @return pointer to the window of 'ctrl_btn'
 */
lv_obj_t * lv_win_get_from_btn(const lv_obj_t * ctrl_btn)
{
    LV_ASSERT_OBJ(ctrl_btn, "lv_btn");

    lv_obj_t * header = lv_obj_get_parent(ctrl_btn);
    lv_obj_t * win    = lv_obj_get_parent(header);

    return win;
}

/**
 * Get the layout of a window
 * @param win pointer to a window object
 * @return the layout of the window (from 'lv_layout_t')
 */
lv_layout_t lv_win_get_layout(lv_obj_t * win)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    return lv_page_get_scrl_layout(ext->page);
}

/**
 * Get the scroll bar mode of a window
 * @param win pointer to a window object
 * @return the scroll bar mode of the window (from 'lv_sb_mode_t')
 */
lv_scrlbar_mode_t lv_win_get_sb_mode(lv_obj_t * win)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    return lv_page_get_sb_mode(ext->page);
}

/**
 * Get focus animation duration
 * @param win pointer to a window object
 * @return duration of animation [ms]
 */
uint16_t lv_win_get_anim_time(const lv_obj_t * win)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    return lv_page_get_anim_time(lv_win_get_content(win));
}

/**
 * Get width of the content area (page scrollable) of the window
 * @param win pointer to a window object
 * @return the width of the content_bg area
 */
lv_coord_t lv_win_get_width(lv_obj_t * win)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext            = lv_obj_get_ext_attr(win);
    lv_obj_t * scrl               = lv_page_get_scrl(ext->page);
    lv_coord_t left = lv_obj_get_style_pad_left(win, LV_WIN_PART_BG);
    lv_coord_t right = lv_obj_get_style_pad_left(win, LV_WIN_PART_BG);

    return lv_obj_get_width_fit(scrl) - left - right;
}

/*=====================
 * Other functions
 *====================*/

/**
 * Focus on an object. It ensures that the object will be visible in the window.
 * @param win pointer to a window object
 * @param obj pointer to an object to focus (must be in the window)
 * @param anim_en LV_ANIM_ON focus with an animation; LV_ANIM_OFF focus without animation
 */
void lv_win_focus(lv_obj_t * win, lv_obj_t * obj, lv_anim_enable_t anim_en)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);
    LV_ASSERT_OBJ(obj, "");


    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    lv_page_focus(ext->page, obj, anim_en);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
/**
 * Handle the drawing related tasks of the window header
 * @param header pointer to an object
 * @param clip_area the object will be drawn only in this area
 * @param mode LV_DESIGN_COVER_CHK: only check if the object fully covers the 'mask_p' area
 *                                  (return 'true' if yes)
 *             LV_DESIGN_DRAW: draw the object (always return 'true')
 *             LV_DESIGN_DRAW_POST: drawing after every children are drawn
 * @param return an element of `lv_design_res_t`
 */
static lv_design_res_t lv_win_header_design(lv_obj_t * header, const lv_area_t * clip_area, lv_design_mode_t mode)
{
    /*Return false if the object is not covers the mask_p area*/
    if(mode == LV_DESIGN_COVER_CHK) {
        return ancestor_header_design(header, clip_area, mode);
    }
    /*Draw the object*/
    else if(mode == LV_DESIGN_DRAW_MAIN) {
        ancestor_header_design(header, clip_area, mode);

        lv_obj_t * win = lv_obj_get_parent(header);
        lv_win_ext_t * ext = lv_obj_get_ext_attr(win);

        lv_style_int_t left = lv_obj_get_style_pad_left(header, LV_OBJ_PART_MAIN);

        lv_draw_label_dsc_t label_dsc;
        lv_draw_label_dsc_init(&label_dsc);
        lv_obj_init_draw_label_dsc(header, LV_OBJ_PART_MAIN, &label_dsc);

        lv_area_t txt_area;
        lv_point_t txt_size;


        lv_txt_get_size(&txt_size, ext->title_txt, label_dsc.font, label_dsc.letter_space, label_dsc.line_space, LV_COORD_MAX,
                        label_dsc.flag);

        txt_area.x1 = header->coords.x1 + left;
        txt_area.y1 = header->coords.y1 + (lv_obj_get_height(header) - txt_size.y) / 2;
        txt_area.x2 = txt_area.x1 + txt_size.x;
        txt_area.y2 = txt_area.y1 + txt_size.y;

        lv_draw_label(&txt_area, clip_area, &label_dsc, ext->title_txt, NULL);
    }
    else if(mode == LV_DESIGN_DRAW_POST) {
        ancestor_header_design(header, clip_area, mode);
    }

    return LV_DESIGN_RES_OK;
}
/**
 * Signal function of the window
 * @param win pointer to a window object
 * @param sign a signal type from lv_signal_t enum
 * @param param pointer to a signal specific variable
 * @return LV_RES_OK: the object is not deleted in the function; LV_RES_INV: the object is deleted
 */
static lv_res_t lv_win_signal(lv_obj_t * win, lv_signal_t sign, void * param)
{
    lv_res_t res;

    if(sign == LV_SIGNAL_GET_STYLE) {
        lv_get_style_info_t * info = param;
        info->result = lv_win_get_style(win, info->part);
        if(info->result != NULL) return LV_RES_OK;
        else return ancestor_signal(win, sign, param);
    }
    else if(sign == LV_SIGNAL_GET_STATE_DSC) {
        lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
        lv_get_state_info_t * info = param;
        if(info->part == LV_WIN_PART_CONTENT_SCRL) info->result = lv_obj_get_state(lv_page_get_scrl(ext->page),
                                                                                       LV_CONT_PART_MAIN);
        else if(info->part == LV_WIN_PART_SCRLBAR) info->result = lv_obj_get_state(ext->page, LV_PAGE_PART_SCRLBAR);
        else if(info->part == LV_WIN_PART_HEADER) info->result = lv_obj_get_state(ext->header, LV_OBJ_PART_MAIN);
        return LV_RES_OK;
    }

    /* Include the ancient signal function */
    res = ancestor_signal(win, sign, param);
    if(res != LV_RES_OK) return res;
    if(sign == LV_SIGNAL_GET_TYPE) return lv_obj_handle_get_type_signal(param, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    if(sign == LV_SIGNAL_CHILD_CHG) { /*Move children to the page*/
        lv_obj_t * page = ext->page;
        if(page != NULL) {
            lv_obj_t * child;
            child = lv_obj_get_child(win, NULL);
            while(child != NULL) {
                if(lv_obj_is_protected(child, LV_PROTECT_PARENT) == false) {
                    lv_obj_t * tmp = child;
                    child          = lv_obj_get_child(win, child); /*Get the next child before move this*/
                    lv_obj_set_parent(tmp, page);
                }
                else {
                    child = lv_obj_get_child(win, child);
                }
            }
        }
    }
    else if(sign == LV_SIGNAL_STYLE_CHG) {
        lv_win_realign(win);
    }
    else if(sign == LV_SIGNAL_COORD_CHG) {
        /*If the size is changed refresh the window*/
        if(lv_area_get_width(param) != lv_obj_get_width(win) || lv_area_get_height(param) != lv_obj_get_height(win)) {
            lv_win_realign(win);
        }
    }
    else if(sign == LV_SIGNAL_CLEANUP) {
        ext->header = NULL; /*These objects were children so they are already invalid*/
        ext->page   = NULL;
        lv_mem_free(ext->title_txt);
        ext->title_txt  = NULL;
    }
    else if(sign == LV_SIGNAL_CONTROL) {
        /*Forward all the control signals to the page*/
        ext->page->signal_cb(ext->page, sign, param);
    }

    return res;
}
/**
 * Get the style descriptor of a part of the object
 * @param win pointer the object
 * @param part the part of the win. (LV_PAGE_WIN_...)
 * @return pointer to the style descriptor of the specified part
 */
static lv_style_list_t * lv_win_get_style(lv_obj_t * win, uint8_t part)
{
    LV_ASSERT_OBJ(win, LV_OBJX_NAME);

    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
    lv_style_list_t * style_dsc_p;

    switch(part) {
        case LV_WIN_PART_BG:
            style_dsc_p = &win->style_list;
            break;
        case LV_WIN_PART_HEADER:
            style_dsc_p = lv_obj_get_style_list(ext->header, LV_OBJ_PART_MAIN);
            break;
        case LV_WIN_PART_SCRLBAR:
            style_dsc_p = lv_obj_get_style_list(ext->page, LV_PAGE_PART_SCRLBAR);
            break;
        case LV_WIN_PART_CONTENT_SCRL:
            style_dsc_p = lv_obj_get_style_list(ext->page, LV_PAGE_PART_SCRL);
            break;
        default:
            style_dsc_p = NULL;
    }

    return style_dsc_p;
}
/**
 * Realign the building elements of a window
 * @param win pointer to a window object
 */
static void lv_win_realign(lv_obj_t * win)
{
    lv_win_ext_t * ext = lv_obj_get_ext_attr(win);

    if(ext->page == NULL || ext->header == NULL) return;

    lv_obj_set_width(ext->header, lv_obj_get_width(win));

    lv_obj_t * btn;
    lv_obj_t * btn_prev = NULL;
    lv_coord_t btn_size = lv_obj_get_height_fit(ext->header);
    lv_style_int_t header_inner = lv_obj_get_style_pad_inner(win, LV_WIN_PART_HEADER);
    lv_style_int_t header_right = lv_obj_get_style_pad_right(win, LV_WIN_PART_HEADER);
    /*Refresh the size of all control buttons*/
    btn = lv_obj_get_child_back(ext->header, NULL);
    while(btn != NULL) {
        lv_obj_set_size(btn, btn_size, btn_size);
        if(btn_prev == NULL) {
            lv_obj_align(btn, ext->header, LV_ALIGN_IN_RIGHT_MID, -header_right, 0);
        }
        else {
            lv_obj_align(btn, btn_prev, LV_ALIGN_OUT_LEFT_MID, - header_inner, 0);
        }
        btn_prev = btn;
        btn      = lv_obj_get_child_back(ext->header, btn);
    }

    lv_obj_set_pos(ext->header, 0, 0);

    lv_obj_set_size(ext->page, lv_obj_get_width(win), lv_obj_get_height(win) - lv_obj_get_height(ext->header));
    lv_obj_align(ext->page, ext->header, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
}

#endif
