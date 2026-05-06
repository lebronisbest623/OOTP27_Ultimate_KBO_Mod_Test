static void kbo_hub_load_skin_assets(void)
{
    if (InterlockedCompareExchange(&g_kbo_hub_skin_assets_loaded, 1, 0) != 0) {
        return;
    }

    char path[MAX_PATH];

    kbo_hub_local_asset_path("github-mark.png", path, sizeof(path));
    g_kbo_hub_asset_github = kbo_hub_load_png_hbitmap_wic(path);
    append_logf("KBO F2 hub github asset path=%s loaded=%d", path, g_kbo_hub_asset_github != NULL);

    kbo_hub_skin_image_path("menu_arrow_right.png", path, sizeof(path));
    g_kbo_hub_asset_menu_arrow = kbo_hub_load_png_hbitmap_wic(path);

    kbo_hub_skin_scrollbar_image_path("sb_bar_top.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_top = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_bar_mid.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_mid = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_bar_bottom.png", path, sizeof(path));
    g_kbo_hub_asset_sb_bar_bottom = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_less_up.png", path, sizeof(path));
    g_kbo_hub_asset_sb_less = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_more_up.png", path, sizeof(path));
    g_kbo_hub_asset_sb_more = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_top.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_top = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_mid.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_mid = kbo_hub_load_png_hbitmap_wic(path);
    kbo_hub_skin_scrollbar_image_path("sb_slider_up_bottom.png", path, sizeof(path));
    g_kbo_hub_asset_sb_slider_bottom = kbo_hub_load_png_hbitmap_wic(path);
}

static void kbo_hub_delete_skin_assets(void)
{
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_github);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_menu_arrow);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_top);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_mid);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_bar_bottom);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_less);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_more);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_top);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_mid);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_sb_slider_bottom);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_league_logo);
    kbo_hub_delete_bitmap(&g_kbo_hub_asset_selected_team_logo);
    g_kbo_hub_logo_cache_league_id = 0;
    g_kbo_hub_logo_cache_team_id   = 0;
    g_kbo_hub_logo_cache_year      = 0;
    g_kbo_hub_skin_assets_loaded   = 0;
}
