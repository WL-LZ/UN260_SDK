cmake_minimum_required(VERSION 3.1)

project(test_lvgl)

# Suppress cmake unused warning
set(ignore ${BUILD_DOC} ${BUILD_DOCS} ${BUILD_EXAMPLE} ${BUILD_EXAMPLES}
 ${BUILD_SHARED_LIBS}${BUILD_TEST}${BUILD_TESTING}${BUILD_TESTS})

add_compile_options(-Wall)

set(CMAKE_C_FLAGS "-O0 -g0")

include_directories(. ./aic_ui)
include_directories(. ./guider_lvgl_demo)
add_executable(${PROJECT_NAME} main.c
	aic_ui/aic_dec.c
	aic_ui/aic_ui.c
	aic_ui/cpu_mem.c
	aic_ui/perf_stats.c
	aic_ui/font/ui_font_Big.c
	aic_ui/font/ui_font_Title.c
	aic_ui/font/ui_font_H1.c
	lv_fbdev.c
	lv_ge2d.c
	lv_port_indev.c
	lv_port_disp.c
    un260/lv_components/lv_components.c
    un260/lv_components/lv_fault_popup.c
    un260/lv_components/lv_loading_orbit.c
    un260/lv_components/lv_print_toast.c
    un260/lv_components/lv_loading_grid.c
    un260/lv_components/lv_debug_overlay.c
    un260/lv_components/lv_upgrade_popup.c
    un260/lv_components/lv_qr_popup.c
    un260/lv_components/smart_island.c
    un260/lv_components/smart_island/smart_island_view.c
    un260/lv_components/smart_island/smart_island_action.c
    un260/lv_components/smart_island/smart_island_warning.c
    un260/lv_components/smart_island/smart_island_result.c
    un260/lv_components/lv_selftest_list.c
    un260/lv_components/qrcodegen.c
    un260/lv_components/lv_capsule_pagination.c
    un260/lv_components/lv_damped_button.c
    un260/lv_components/lv_content_pager.c
    un260/app_service/setting_service.c
    un260/lv_system/app_clock.c
    un260/app_service/app_boot_runtime.c
    un260/app_service/app_command_runtime.c
    un260/app_service/app_counting_runtime.c
    un260/app_service/app_currency_runtime.c
    un260/app_service/app_protocol_runtime.c
    un260/app_service/app_serial_runtime.c
    un260/app_service/app_setting_runtime.c
    un260/app_service/app_ui_runtime.c
    un260/boot/boot_service.c
    un260/boot/boot_reply.c
    un260/counting/counting_control_reply.c
    un260/counting/counting_action_service.c
    un260/counting/counting_data_store.c
    un260/counting/counting_denom_query_service.c
    un260/counting/counting_denom_reply.c
    un260/counting/counting_history_service.c
    un260/counting/counting_info_reply.c
    un260/counting/counting_reject_analysis_service.c
    un260/counting/counting_reject_reason.c
    un260/counting/counting_reject_sn_reply.c
    un260/innovation/multi_pass_verification.c
    un260/innovation/page_32_innovation.c
    un260/gesture/gesture_service.c
    un260/gesture/gesture_guide.c
    un260/device_info/device_info.c
    un260/data_collection/data_collection.c
    un260/diagnostic/diagnostic.c
    un260/history/history_export_sn_parser.c
    un260/history/history_export_text.c
    un260/currency/currency_state.c
    un260/currency/currency_service.c
    un260/currency/currency_reply.c
    un260/serial_number/serial_number.c
    un260/storage/usb_storage.c
    un260/print/print_config.c
    un260/cfd/cfd.c
    un260/protocol/mode_codec.c
    un260/protocol/protocol_frame.c
    un260/protocol/protocol_frame_parser.c
    un260/protocol/protocol_frame_queue.c
    un260/protocol/protocol_rx_service.c
    un260/protocol/protocol_request.c
    un260/app_service/app_setting_reply_basic.c
    un260/app_service/app_setting_reply_detail.c
    un260/protocol/startup_sync_reply.c
    un260/protocol/auxiliary_reply.c
    un260/protocol/stream_data_reply.c
    un260/machine_state/machine_state.c
    un260/lv_core/lv_page_event.c
    un260/lv_core/lv_page_manager.c
    un260/lv_core/page_01_main.c
    un260/lv_core/page_01_detail_scroll.c
    un260/lv_core/page_02_list.c
    un260/lv_core/page_03_menu.c
    un260/lv_core/page_05_set_password.c
    un260/lv_core/page_06_settings.c
    un260/lv_core/settings_detail_ui.c
    un260/lv_core/page_07_curr.c
    un260/lv_core/page_07_curr/page_07_curr_model.c
    un260/lv_core/page_08_boot.c
    un260/lv_core/page_09_cis_cala.c 
    un260/lv_core/page_10_debug.c 
    un260/lv_core/page_11_timeset.c
    un260/lv_core/page_12_sensor.c
    un260/lv_core/page_13_upgrade.c
    un260/lv_core/page_14_main_upgrade.c
    un260/lv_core/page_15_image_upgrade.c
    un260/lv_core/page_16_ui_upgrade.c
    un260/lv_core/upgrade_page_runtime.c
    un260/lv_core/ui_upgrade_service.c
    un260/lv_core/page_17_motor_test.c
    un260/lv_core/page_18_pure.c
    un260/lv_core/page_19_history.c
    un260/lv_core/page_20_set_print.c
    un260/lv_core/page_21_set_language.c
    un260/lv_core/page_22_set_double_note.c
    un260/lv_core/page_23_set_flap.c
    un260/lv_core/page_24_set_reject_pocket.c
    un260/lv_core/page_25_set_serial_number.c
    un260/lv_core/page_26_set_aging.c
    un260/lv_core/page_27_set_cfd_level.c
    un260/lv_core/page_28_get_image.c
    un260/lv_core/page_29_set_password.c
    un260/lv_core/page_30_set_factory.c
    un260/lv_core/page_31_get_wave.c
    un260/lv_core/page_00_boot_anim.c
    un260/lv_core/boot_anim/page_00_boot_anim_theme_a.c
    
    un260/lv_resources/lv_img_init.c
    un260/lv_system/platform_app.c
    un260/lv_system/ui_state_store.c
    un260/lv_system/ui_state_runtime.c
    un260/lv_system/lv_str.c
    un260/lv_system/ui_lang.c
    un260/lv_system/ui_text_page.c
    un260/lv_system/ui_text_widget.c
    un260/lv_system/ui_qr_data.c
    un260/lv_system/ui_screenshot.c
    un260/lv_system/ui_screen_recording.c
    un260/lv_system/ui_export_data.c
    un260/lv_system/ui_history_data_fs.c
    un260/lv_system/ui_history_export_data.c
    un260/lv_system/user_cfg.c
    un260/lv_system/machine_time.c
    un260/recording/screen_recording_service.c
    un260/lv_drivers/lv_drivers.c
    un260/lv_drivers/uart_io.c
    un260/lv_drivers/uart_bridge_service.c
    #font
	un260/font/lv_font_montserrat_8.c 
    un260/font/lv_font_montserrat_10.c
    un260/font/lv_font_montserrat_12_subpx.c
    un260/font/lv_font_montserrat_12.c
    un260/font/lv_font_montserrat_14.c
    un260/font/lv_font_montserrat_16.c
    un260/font/lv_font_montserrat_18.c
    un260/font/lv_font_montserrat_20.c
    un260/font/lv_font_montserrat_22.c
    un260/font/lv_font_montserrat_24.c
    un260/font/lv_font_montserrat_26.c
    un260/font/lv_font_montserrat_28_compressed.c
    un260/font/lv_font_montserrat_28.c
    un260/font/lv_font_montserrat_30.c
    un260/font/lv_font_montserrat_32.c
    un260/font/lv_font_montserrat_34.c
    un260/font/lv_font_montserrat_36.c
    un260/font/lv_font_montserrat_38.c
    un260/font/lv_font_montserrat_40.c
    un260/font/lv_font_montserrat_42.c
    un260/font/lv_font_montserrat_44.c
    un260/font/lv_font_montserrat_46.c
    un260/font/lv_font_montserrat_48.c
    un260/font/MONTSERRAT_EXTRABOLD_35.c
    un260/font/MONTSERRAT_MEDIUM_15.c
    un260/font/lv_font_rajdhani_27.c
    un260/font/lv_font_rajdhani_142.c
    un260/font/lv_font_rajdhani_194.c
    un260/font/lv_font_manrope_bold_10.c
    un260/font/lv_font_manrope_bold_12.c
    un260/font/lv_font_manrope_bold_14.c
    un260/font/lv_font_manrope_bold_16.c
    un260/font/lv_font_manrope_bold_18.c
    un260/font/lv_font_manrope_bold_20.c
    un260/font/lv_font_manrope_bold_22.c
    un260/font/lv_font_manrope_bold_24.c
    un260/font/lv_font_manrope_bold_28.c
    un260/font/lv_font_manrope_bold_30.c
    un260/font/lv_font_manrope_bold_32.c
    un260/font/lv_font_manrope_bold_40.c
    un260/font/lv_font_manrope_bold_42.c
    un260/font/lv_font_manrope_bold_44.c
    un260/font/lv_font_manrope_bold_48.c
    un260/font/lv_font_manrope_extrabold_10.c
    un260/font/lv_font_manrope_extrabold_12.c
    un260/font/lv_font_manrope_extrabold_14.c
    un260/font/lv_font_manrope_extrabold_16.c
    un260/font/lv_font_manrope_extrabold_18.c
    un260/font/lv_font_manrope_extrabold_20.c
    un260/font/lv_font_manrope_extrabold_22.c
    un260/font/lv_font_manrope_extrabold_24.c
    un260/font/lv_font_manrope_extrabold_28.c
    un260/font/lv_font_manrope_extrabold_30.c
    un260/font/lv_font_manrope_extrabold_32.c
    un260/font/lv_font_manrope_extrabold_40.c
    un260/font/lv_font_manrope_extrabold_42.c
    un260/font/lv_font_manrope_extrabold_44.c
    un260/font/lv_font_manrope_extrabold_48.c
    un260/font/lv_font_instrument_sans_bold_10.c
    un260/font/lv_font_instrument_sans_bold_12.c
    un260/font/lv_font_instrument_sans_bold_14.c
    un260/font/lv_font_instrument_sans_bold_16.c
    un260/font/lv_font_instrument_sans_bold_18.c
    un260/font/lv_font_instrument_sans_bold_20.c
    un260/font/lv_font_instrument_sans_bold_22.c
    un260/font/lv_font_instrument_sans_bold_24.c
    un260/font/lv_font_instrument_sans_bold_28.c
    un260/font/lv_font_instrument_sans_bold_30.c
    un260/font/lv_font_instrument_sans_bold_32.c
    un260/font/lv_font_instrument_sans_bold_40.c
    un260/font/lv_font_instrument_sans_bold_42.c
    un260/font/lv_font_instrument_sans_bold_44.c
    un260/font/lv_font_instrument_sans_bold_48.c
    un260/font/lv_font_instrument_sans_medium_10.c
    un260/font/lv_font_instrument_sans_medium_12.c
    un260/font/lv_font_instrument_sans_medium_14.c
    un260/font/lv_font_instrument_sans_medium_16.c
    un260/font/lv_font_instrument_sans_medium_18.c
    un260/font/lv_font_instrument_sans_medium_20.c
    un260/font/lv_font_instrument_sans_medium_22.c
    un260/font/lv_font_instrument_sans_medium_24.c
    un260/font/lv_font_instrument_sans_medium_28.c
    un260/font/lv_font_instrument_sans_medium_30.c
    un260/font/lv_font_instrument_sans_medium_32.c
    un260/font/lv_font_instrument_sans_medium_40.c
    un260/font/lv_font_instrument_sans_medium_42.c
    un260/font/lv_font_instrument_sans_medium_44.c
    un260/font/lv_font_instrument_sans_medium_48.c
    un260/font/lv_font_instrument_sans_semibold_10.c
    un260/font/lv_font_instrument_sans_semibold_12.c
    un260/font/lv_font_instrument_sans_semibold_14.c
    un260/font/lv_font_instrument_sans_semibold_16.c
    un260/font/lv_font_instrument_sans_semibold_18.c
    un260/font/lv_font_instrument_sans_semibold_20.c
    un260/font/lv_font_instrument_sans_semibold_22.c
    un260/font/lv_font_instrument_sans_semibold_24.c
    un260/font/lv_font_instrument_sans_semibold_28.c
    un260/font/lv_font_instrument_sans_semibold_30.c
    un260/font/lv_font_instrument_sans_semibold_32.c
    un260/font/lv_font_instrument_sans_semibold_40.c
    un260/font/lv_font_instrument_sans_semibold_42.c
    un260/font/lv_font_instrument_sans_semibold_44.c
    un260/font/lv_font_instrument_sans_semibold_48.c
    # img
    un260/lv_resources/page_01_back.c
    un260/lv_resources/page_01_collect_icon.c
    un260/lv_resources/page_01_curr_USD.c
    un260/lv_resources/page_01_esc_icon.c
    un260/lv_resources/page_01_list_icon.c
    un260/lv_resources/page_01_menu_icon.c
    un260/lv_resources/page_01_mode_icon.c
    un260/lv_resources/page_01_print_icon.c
    un260/lv_resources/page_01_set_icon.c
    un260/lv_resources/page_01_start_icon.c
    un260/lv_resources/page_01_usb_icon.c
    un260/lv_resources/page_02_home_icon.c
    un260/lv_resources/page_02_list_img.c
    un260/lv_resources/page_03_del_icon.c
    un260/lv_resources/page_03_menu_bg_img.c
    un260/lv_resources/page_03_ok_icon.c
    un260/lv_resources/page_05_set_password_bg_img.c
    un260/lv_resources/page_07_currency_bg_img.c
    #img_curr
    un260/lv_resources/lv_curr/CURR_CNY.c
    un260/lv_resources/lv_curr/CURR_EGP.c
    un260/lv_resources/lv_curr/CURR_EUR.c
    un260/lv_resources/lv_curr/CURR_GBP.c
    un260/lv_resources/lv_curr/CURR_ISK.c
    un260/lv_resources/lv_curr/CURR_PHP.c
    un260/lv_resources/lv_curr/CURR_SOS.c
    un260/lv_resources/lv_curr/CURR_TRY.c
    un260/lv_resources/lv_curr/CURR_USD.c

)

# Find libts for link
if ( ${USE_RTP_TSLIB} STREQUAL "yes")
	add_definitions(-DUSE_RTP_TSLIB)
	find_library(LIBTS libts.so)
	target_link_libraries(${PROJECT_NAME} ${LIBTS})
endif()

find_library(LIBMPP_GE libmpp_ge.so)
find_library(LIBMPP_BASE libmpp_base.so)
find_library(LIBMPP_VE libmpp_ve.so)
find_library(LIBMPP_DEC libmpp_decoder.so)
find_library(LIBMPP_ENC libmpp_encoder.so)
target_link_libraries(${PROJECT_NAME} ${LIBMPP_GE} ${LIBMPP_DEC} ${LIBMPP_ENC} ${LIBMPP_BASE} ${LIBMPP_VE})
#target_link_libraries(${PROJECT_NAME} lvgl lvgl_examples lvgl_demos)
target_link_libraries(${PROJECT_NAME} lvgl )
target_link_libraries(${PROJECT_NAME} media_player)
target_link_libraries(${PROJECT_NAME} m)
target_link_libraries(${PROJECT_NAME} pthread)

set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "/usr/local/lib")

include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_FULL_LIBDIR)
	install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
endif()

add_definitions(-DLVGL_DIR="L:${CMAKE_INSTALL_FULL_DATAROOTDIR}/lvgl_data/")

if(DEFINED CMAKE_INSTALL_FULL_DATAROOTDIR)
	install(DIRECTORY aic_ui/lvgl_data/
		DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/lvgl_data)
endif() # CMAKE_INSTALL_FULL_DATAROOTDIR
