#include "ui_text.h"
#include "ui_lang.h"
#include <stddef.h>

extern const ui_text_item_t g_ui_text_page_group[UI_TEXT_PAGE_MAX];

static const ui_text_item_t g_ui_text_widget_group[UI_TEXT_MAX - UI_TEXT_WIDGET_BASE] = {
    //升级弹窗
    [UI_TEXT_WIDGET_UPGRADE_POPUP_TAG - UI_TEXT_WIDGET_BASE] = {"SYSTEM UPGRADE", "系统升级", "시스템 업그레이드"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_TITLE - UI_TEXT_WIDGET_BASE] = {"Upgrade package detected", "检测到升级包", "업그레이드 패키지 감지됨"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_DESC - UI_TEXT_WIDGET_BASE] = {
        "A USB drive with an upgrade package was detected. Do not power off the device or remove the USB drive during the upgrade.",
        "检测到包含升级包的U盘。升级期间请勿断电或移除U盘。",
        "업그레이드 패키지가 포함된 USB 드라이브가 감지되었습니다. 업그레이드 중에는 전원을 끄거나 USB 드라이브를 제거하지 마십시오."
    },
    [UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_BTN_CANCEL - UI_TEXT_WIDGET_BASE] = {"Cancel", "取消", "취소"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_PROMPT_BTN_CONFIRM - UI_TEXT_WIDGET_BASE] = {"Upgrade Now", "立即升级", "지금 업그레이드"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_TITLE - UI_TEXT_WIDGET_BASE] = {"Updating system", "正在升级系统", "시스템 업그레이드 중"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_DESC - UI_TEXT_WIDGET_BASE] = {"Do not power off or remove the USB drive", "请勿断电或移除U盘", "전원을 끄거나 USB 드라이브를 제거하지 마십시오"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_STEP_VERIFY - UI_TEXT_WIDGET_BASE] = {"Verifying upgrade package", "正在校验升级包", "업그레이드 패키지 확인 중"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_PROGRESS_STEP_VERIFY_FAIL - UI_TEXT_WIDGET_BASE] = {"Upgrade package verification failed", "升级包校验失败", "업그레이드 패키지 확인 실패"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_TITLE - UI_TEXT_WIDGET_BASE] = {"Upgrade complete", "升级完成", "업그레이드 완료"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_DESC - UI_TEXT_WIDGET_BASE] = {"The system has been updated successfully. Reboot the device now?", "系统已升级成功，是否现在重启设备？", "시스템이 성공적으로 업데이트되었습니다. 지금 장치를 재부팅하시겠습니까?"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_BTN_LATER - UI_TEXT_WIDGET_BASE] = {"Later", "稍后重启", "나중에"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_SUCCESS_BTN_CONFIRM - UI_TEXT_WIDGET_BASE] = {"Confirm Reboot", "确认重启", "재부팅 확인"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_TITLE - UI_TEXT_WIDGET_BASE] = {"Upgrade failed", "升级失败", "업그레이드 실패"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_DESC - UI_TEXT_WIDGET_BASE] = {"The upgrade package is invalid. Please check the file and try again.", "升级文件异常，请检查后重试。", "업그레이드 패키지에 문제가 있습니다. 파일을 확인한 후 다시 시도하십시오."},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_BTN_RETRY - UI_TEXT_WIDGET_BASE] = {"Retry Detection", "重新检测", "다시 검색"},
    [UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_START_SCRIPT - UI_TEXT_WIDGET_BASE] = {"Failed to start the upgrade script. Please try again.", "升级脚本启动失败，请重试。", "업그레이드 스크립트를 시작하지 못했습니다. 다시 시도하십시오."},

    //打印弹窗
    [UI_TEXT_WIDGET_PRINT_TOAST_PRINTING - UI_TEXT_WIDGET_BASE] = {"Printing...", "正在打印...", "인쇄 중..."},
    [UI_TEXT_WIDGET_PRINT_TOAST_COUNT_FIRST - UI_TEXT_WIDGET_BASE] = {"Please count first", "请先点钞", "먼저 계수해 주세요"},

    //二维码弹窗
    [UI_TEXT_WIDGET_QR_POPUP_TITLE - UI_TEXT_WIDGET_BASE] = {"COUNTING QR", "点钞二维码", "계수 QR"},
    [UI_TEXT_WIDGET_QR_POPUP_DESC - UI_TEXT_WIDGET_BASE] = {"Scan to view details", "扫码查看详情", "스캔하여 상세보기"},
    [UI_TEXT_WIDGET_QR_POPUP_BTN_CLOSE - UI_TEXT_WIDGET_BASE] = {"Close", "关闭", "닫기"},
    [UI_TEXT_WIDGET_QR_POPUP_NO_DATA - UI_TEXT_WIDGET_BASE] = {"No counting data", "暂无点钞数据", "계수 데이터가 없습니다"},
    [UI_TEXT_WIDGET_QR_POPUP_DATA_TOO_LARGE - UI_TEXT_WIDGET_BASE] = {"QR data is too large", "二维码数据过大", "QR 데이터가 너무 큽니다"},
    [UI_TEXT_WIDGET_QR_DATA_TIME - UI_TEXT_WIDGET_BASE] = {"TIME", "时间", "시간"},
    [UI_TEXT_WIDGET_QR_DATA_CUR - UI_TEXT_WIDGET_BASE] = {"CUR", "币种", "통화"},
    [UI_TEXT_WIDGET_QR_DATA_AMOUNT - UI_TEXT_WIDGET_BASE] = {"AMOUNT", "金额", "금액"},
    [UI_TEXT_WIDGET_QR_DATA_PCS - UI_TEXT_WIDGET_BASE] = {"PCS", "张数", "매수"},
    [UI_TEXT_WIDGET_QR_DATA_SN - UI_TEXT_WIDGET_BASE] = {"SN", "冠字号", "일련번호"},
    [UI_TEXT_WIDGET_QR_DATA_ERR - UI_TEXT_WIDGET_BASE] = {"ERR", "报错", "오류"},

    //灵动岛动作页
    [UI_TEXT_WIDGET_SMART_ISLAND_ACTION_QR - UI_TEXT_WIDGET_BASE] = {"QR Export", "二维码导出", "QR 내보내기"},
    [UI_TEXT_WIDGET_SMART_ISLAND_ACTION_TIME - UI_TEXT_WIDGET_BASE] = {"Export Data", "导出当前数据", "데이터 내보내기"},
    [UI_TEXT_WIDGET_SMART_ISLAND_ACTION_REPORT - UI_TEXT_WIDGET_BASE] = {"Report", "报表", "리포트"},
    [UI_TEXT_WIDGET_SMART_ISLAND_ACTION_SELFTEST - UI_TEXT_WIDGET_BASE] = {"Self-test", "自检", "자가 점검"},
    [UI_TEXT_WIDGET_SMART_ISLAND_ACTION_POPUP_ON - UI_TEXT_WIDGET_BASE] = {"Popup: ON", "弹窗: 开", "팝업: 켜짐"},
    [UI_TEXT_WIDGET_SMART_ISLAND_ACTION_POPUP_OFF - UI_TEXT_WIDGET_BASE] = {"Popup: OFF", "弹窗: 关", "팝업: 꺼짐"},
    [UI_TEXT_WIDGET_SMART_ISLAND_ACTION_FUNC3 - UI_TEXT_WIDGET_BASE] = {"Popup", "弹窗", "팝업"},
    [UI_TEXT_WIDGET_SMART_ISLAND_ACTION_FUNC4 - UI_TEXT_WIDGET_BASE] = {"Pure Count", "纯净跑钞", "순수 계수"},

    //灵动岛状态文案
    [UI_TEXT_WIDGET_SMART_ISLAND_READY_TITLE - UI_TEXT_WIDGET_BASE] = {"Ready", "就绪", "준비됨"},
    [UI_TEXT_WIDGET_SMART_ISLAND_READY_SUBTITLE - UI_TEXT_WIDGET_BASE] = {"System OK", "系统正常", "시스템 정상"},
    [UI_TEXT_WIDGET_SMART_ISLAND_EXPAND_TITLE - UI_TEXT_WIDGET_BASE] = {"SYSTEM STATUS", "系统状态", "시스템 상태"},
    [UI_TEXT_WIDGET_SMART_ISLAND_EXPAND_SUBTITLE - UI_TEXT_WIDGET_BASE] = {"ALL ACTIVE", "全部正常", "모두 정상"},
    [UI_TEXT_WIDGET_SMART_ISLAND_COUNTING_TITLE - UI_TEXT_WIDGET_BASE] = {"Counting...", "正在点钞...", "계수 중..."},
    [UI_TEXT_WIDGET_SMART_ISLAND_COUNTING_SUBTITLE - UI_TEXT_WIDGET_BASE] = {"Running", "运行中", "실행 중"},
    [UI_TEXT_WIDGET_SMART_ISLAND_RUNNING_TITLE - UI_TEXT_WIDGET_BASE] = {"Running...", "正在运行...", "실행 중..."},
    [UI_TEXT_WIDGET_SMART_ISLAND_RUNNING_SUBTITLE - UI_TEXT_WIDGET_BASE] = {"Check banknotes", "请检查纸钞", "지폐를 확인해 주세요"},
    [UI_TEXT_WIDGET_SMART_ISLAND_WARNING_TITLE - UI_TEXT_WIDGET_BASE] = {"Warning", "警告", "경고"},
    [UI_TEXT_WIDGET_SMART_ISLAND_WARNING_SUBTITLE - UI_TEXT_WIDGET_BASE] = {"Warning", "警告", "경고"},
    [UI_TEXT_WIDGET_SMART_ISLAND_RESULT_TITLE - UI_TEXT_WIDGET_BASE] = {"Count Finished", "点钞完成", "계수 완료"},
    [UI_TEXT_WIDGET_SMART_ISLAND_RESULT_SUBTITLE - UI_TEXT_WIDGET_BASE] = {"Done", "完成", "완료"},
    [UI_TEXT_WIDGET_SMART_ISLAND_COUNT_FINISHED - UI_TEXT_WIDGET_BASE] = {"Count Finished", "点钞完成", "계수 완료"},
    [UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR - UI_TEXT_WIDGET_BASE] = {"Count Error", "点钞错误", "계수 오류"},
    [UI_TEXT_WIDGET_SMART_ISLAND_UPDATE_TITLE - UI_TEXT_WIDGET_BASE] = {"Updating...", "正在升级...", "업데이트 중..."},
    [UI_TEXT_WIDGET_SMART_ISLAND_UPDATE_SUBTITLE - UI_TEXT_WIDGET_BASE] = {"Please wait", "请稍候", "잠시만 기다려 주세요"},
    [UI_TEXT_WIDGET_SMART_ISLAND_QR_READY - UI_TEXT_WIDGET_BASE] = {"QR Ready", "二维码就绪", "QR 준비됨"},
    [UI_TEXT_WIDGET_SMART_ISLAND_QR_SUBTITLE - UI_TEXT_WIDGET_BASE] = {"Tap to continue", "点击继续", "탭하여继续"},
    [UI_TEXT_WIDGET_SMART_ISLAND_IDLE_INFO_TITLE - UI_TEXT_WIDGET_BASE] = {"Ready to count", "准备点钞", "계수 준비 완료"},
    [UI_TEXT_WIDGET_SMART_ISLAND_COUNTING_INFO_TITLE - UI_TEXT_WIDGET_BASE] = {"Counting in progress", "正在点钞", "계수 진행 중"},
    [UI_TEXT_WIDGET_SMART_ISLAND_WARNING_INFO_TITLE - UI_TEXT_WIDGET_BASE] = {"Attention required", "需要处理", "확인이 필요합니다"},
    [UI_TEXT_WIDGET_SMART_ISLAND_WARNING_INFO_FOOTER - UI_TEXT_WIDGET_BASE] = {"Open details to check", "打开详情查看", "상세 내용을 확인하세요"},
    [UI_TEXT_WIDGET_SMART_ISLAND_RESULT_INFO_FOOTER_OK - UI_TEXT_WIDGET_BASE] = {"No suspect note", "未发现可疑钞", "의심 지폐 없음"},
    [UI_TEXT_WIDGET_SMART_ISLAND_RESULT_INFO_FOOTER_REJECT_FMT - UI_TEXT_WIDGET_BASE] = {"%u reject notes", "%u 条退钞", "%u개 리젝트"},
    [UI_TEXT_WIDGET_SMART_ISLAND_QR_INFO_SUBTITLE - UI_TEXT_WIDGET_BASE] = {"Scan to export result", "扫码导出本次结果", "스캔하여 결과를 내보내기"},
    [UI_TEXT_WIDGET_SMART_ISLAND_QR_INFO_FOOTER - UI_TEXT_WIDGET_BASE] = {"Data prepared successfully", "数据已准备完成", "데이터 준비 완료"},
    [UI_TEXT_WIDGET_SMART_ISLAND_UPDATE_INFO_FOOTER - UI_TEXT_WIDGET_BASE] = {"Please keep power on", "请保持设备通电", "전원을 유지해 주세요"},
    [UI_TEXT_WIDGET_SMART_ISLAND_MODE_AUTO - UI_TEXT_WIDGET_BASE] = {"Auto mode", "自动模式", "자동 모드"},
    [UI_TEXT_WIDGET_SMART_ISLAND_MODE_MANUAL - UI_TEXT_WIDGET_BASE] = {"Manual feed", "手动模式", "수동 급지"},
    [UI_TEXT_WIDGET_SMART_ISLAND_BATCH_ACTIVE_FMT - UI_TEXT_WIDGET_BASE] = {"Batch %d active", "预置 %d 已启用", "배치 %d 활성화"},
    [UI_TEXT_WIDGET_SMART_ISLAND_BATCH_PROGRESS_FMT - UI_TEXT_WIDGET_BASE] = {"Batch %d / %d", "预置 %d / %d", "배치 %d / %d"},
    [UI_TEXT_WIDGET_SMART_ISLAND_CUR_MODE_FMT - UI_TEXT_WIDGET_BASE] = {"%s - %s", "%s - %s", "%s - %s"},
    [UI_TEXT_WIDGET_SMART_ISLAND_CUR_PCS_FMT - UI_TEXT_WIDGET_BASE] = {"%s - %d pcs", "%s - %d 张", "%s - %d장"},
    [UI_TEXT_WIDGET_SMART_ISLAND_CUR_PCS_AMOUNT_FMT - UI_TEXT_WIDGET_BASE] = {"%s - %d pcs - %.0f", "%s - %d 张 - %.0f", "%s - %d장 - %.0f"},
    [UI_TEXT_WIDGET_SMART_ISLAND_READY_CUR_FMT - UI_TEXT_WIDGET_BASE] = {"Ready - %s", "就绪 - %s", "준비 - %s"},
    [UI_TEXT_WIDGET_SMART_ISLAND_COUNTING_PCS_FMT - UI_TEXT_WIDGET_BASE] = {"Counting - %d pcs", "点钞中 - %d 张", "계수 중 - %d장"},
    [UI_TEXT_WIDGET_SMART_ISLAND_SERIAL_WAITING - UI_TEXT_WIDGET_BASE] = {"Serial number...", "冠字号...", "일련번호..."},
    [UI_TEXT_WIDGET_SMART_ISLAND_LAST_TAG - UI_TEXT_WIDGET_BASE] = {"Last", "上一把", "이전"},
    [UI_TEXT_WIDGET_SMART_ISLAND_PCS_AMOUNT_FMT - UI_TEXT_WIDGET_BASE] = {
        "#FFFFFF PCS# #60A5FA %d# #737373 |# #FFFFFF Amount# #60A5FA %.0f#",
        "#FFFFFF 张数# #60A5FA %d# #737373 |# #FFFFFF 金额# #60A5FA %.0f#",
        "#FFFFFF 매수# #60A5FA %d# #737373 |# #FFFFFF 금액# #60A5FA %.0f#"
    },
    [UI_TEXT_WIDGET_SMART_ISLAND_IDLE_NO_COUNT - UI_TEXT_WIDGET_BASE] = {
        "No counting yet",
        "未跑钞",
        "계수 이력 없음"
    },
    [UI_TEXT_WIDGET_SMART_ISLAND_PLACE_BANKNOTES - UI_TEXT_WIDGET_BASE] = {
        "Please place banknotes",
        "请放入钞票",
        "지폐를 넣어 주세요"
    },
    [UI_TEXT_WIDGET_SMART_ISLAND_RESULT_OK_TITLE - UI_TEXT_WIDGET_BASE] = {"All notes normal", "钞票全部正常", "모든 지폐 정상"},
    [UI_TEXT_WIDGET_SMART_ISLAND_RESULT_OK_DETAIL - UI_TEXT_WIDGET_BASE] = {"All notes accepted", "全部钞票通过", "모든 지폐 정상"},
    [UI_TEXT_WIDGET_SMART_ISLAND_RESULT_ISSUE_TITLE - UI_TEXT_WIDGET_BASE] = {"Abnormal notes found", "发现异常钞票", "이상 지폐 발견"},
    [UI_TEXT_WIDGET_SMART_ISLAND_RESULT_ISSUE_DETAIL_FMT - UI_TEXT_WIDGET_BASE] = {
        "#FF5A5F %d# #FFFFFF Suspect# #737373 |# #FF5A5F %d# #FFFFFF Damaged#",
        "#FF5A5F %d# #FFFFFF 可疑# #737373 |# #FF5A5F %d# #FFFFFF 破损#",
        "#FF5A5F %d# #FFFFFF 의심# #737373 |# #FF5A5F %d# #FFFFFF 손상#"
    },
    [UI_TEXT_WIDGET_SMART_ISLAND_NO_REJECT - UI_TEXT_WIDGET_BASE] = {"No reject note", "无退钞", "리젝트 없음"},
    [UI_TEXT_WIDGET_SMART_ISLAND_SWIPE_ACTIONS - UI_TEXT_WIDGET_BASE] = {"Swipe for actions", "滑动查看功能", "밀어서 기능 보기"},
    [UI_TEXT_WIDGET_SMART_ISLAND_SETTINGS_SAVED - UI_TEXT_WIDGET_BASE] = {"Settings saved", "设置已保存", "설정 저장됨"},

    //截屏功能
    [UI_TEXT_WIDGET_SCREENSHOT_LABEL - UI_TEXT_WIDGET_BASE] = {"SCREENSHOT", "截屏", "스크린샷"},
    [UI_TEXT_WIDGET_SCREENSHOT_SAVED - UI_TEXT_WIDGET_BASE] = {"Screenshot saved to USB", "截屏已保存到U盘", "스크린샷이 USB에 저장되었습니다"},
    [UI_TEXT_WIDGET_SCREENSHOT_INSERT_USB - UI_TEXT_WIDGET_BASE] = {"Please insert USB drive", "请插入U盘", "USB를 삽입해 주세요"},
    [UI_TEXT_WIDGET_SCREENSHOT_SAVE_FAILED - UI_TEXT_WIDGET_BASE] = {"Screenshot save failed", "截屏保存失败", "스크린샷 저장 실패"},

    //录屏功能
    [UI_TEXT_WIDGET_SCREEN_RECORDING_LABEL - UI_TEXT_WIDGET_BASE] = {"RECORD", "录屏", "화면 녹화"},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_STARTED - UI_TEXT_WIDGET_BASE] = {"Screen recording started", "录屏已开始", "화면 녹화를 시작했습니다"},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_STOPPING - UI_TEXT_WIDGET_BASE] = {"Saving screen recording...", "正在保存录屏…", "화면 녹화를 저장하는 중입니다..."},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_SAVED - UI_TEXT_WIDGET_BASE] = {"Screen recording saved to USB", "录屏已保存到U盘", "화면 녹화가 USB에 저장되었습니다"},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_FAILED - UI_TEXT_WIDGET_BASE] = {"Screen recording failed", "录屏失败", "화면 녹화에 실패했습니다"},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_FPS_TITLE - UI_TEXT_WIDGET_BASE] = {"SELECT RECORDING FRAME RATE", "选择录屏帧率", "녹화 프레임 속도 선택"},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_FPS_HINT - UI_TEXT_WIDGET_BASE] = {"24 FPS is recommended for smooth recording and stable UI performance.", "推荐24帧，可兼顾录屏流畅度与界面性能。", "부드러운 녹화와 안정적인 UI 성능을 위해 24 FPS를 권장합니다."},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_FPS_8 - UI_TEXT_WIDGET_BASE] = {"8 FPS", "8 FPS", "8 FPS"},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_FPS_24 - UI_TEXT_WIDGET_BASE] = {"24 FPS", "24 FPS", "24 FPS"},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_FPS_60 - UI_TEXT_WIDGET_BASE] = {"60 FPS", "60 FPS", "60 FPS"},
    [UI_TEXT_WIDGET_SCREEN_RECORDING_CANCEL - UI_TEXT_WIDGET_BASE] = {"CANCEL", "取消", "취소"},
    [UI_TEXT_WIDGET_PERFORMANCE_MONITOR_LABEL - UI_TEXT_WIDGET_BASE] = {"PERFORMANCE", "性能监视", "성능 모니터"},
    [UI_TEXT_WIDGET_PERFORMANCE_PROFILE_LABEL - UI_TEXT_WIDGET_BASE] = {"PROFILE LOG", "性能日志", "성능 로그"},

    //pure页面
    [UI_TEXT_WIDGET_PURE_AMOUNT - UI_TEXT_WIDGET_BASE] = {"A M O U N T", "金 额", "금 액"},
    [UI_TEXT_WIDGET_PURE_PCS - UI_TEXT_WIDGET_BASE] = {"P C S", "张 数", "매 수"},
    [UI_TEXT_WIDGET_PURE_TOTAL - UI_TEXT_WIDGET_BASE] = {"T O T A L", "总 计", "합 계"},
    [UI_TEXT_WIDGET_PURE_VALUE - UI_TEXT_WIDGET_BASE] = {"V A L U E", "金 额", "금 액"},
    [UI_TEXT_WIDGET_PURE_PIECES - UI_TEXT_WIDGET_BASE] = {"P I E C E S", "张 数", "매 수"},
    [UI_TEXT_WIDGET_PURE_REJECT - UI_TEXT_WIDGET_BASE] = {"R E J E C T", "退 钞", "리젝트"},
    [UI_TEXT_WIDGET_PURE_START - UI_TEXT_WIDGET_BASE] = {"S T A R T", "开 始", "시 작"},
    [UI_TEXT_WIDGET_PURE_CLEAR - UI_TEXT_WIDGET_BASE] = {"C L E A R", "清 空", "초 기 화"},

    //故障弹窗（no-note）
    [UI_TEXT_WIDGET_FAULT_START_FAILED - UI_TEXT_WIDGET_BASE] = {"START COUNT FAILED", "启动点钞失败", "계수 시작 실패"},
    [UI_TEXT_WIDGET_FAULT_START_COUNT_ERROR - UI_TEXT_WIDGET_BASE] = {"START COUNT ERROR", "启动点钞错误", "계수 시작 오류"},
    [UI_TEXT_WIDGET_FAULT_NO_NOTE_MAIN - UI_TEXT_WIDGET_BASE] = {"No Banknotes Detected", "未检测到纸钞", "지폐가 감지되지 않았습니다"},
    [UI_TEXT_WIDGET_FAULT_NO_NOTE_REASON - UI_TEXT_WIDGET_BASE] = {"The machine is normal, but no banknotes were detected.", "设备状态正常，但未检测到纸钞。", "장비 상태는 정상이지만 지폐가 감지되지 않았습니다."},
    [UI_TEXT_WIDGET_FAULT_NO_NOTE_SOLUTION - UI_TEXT_WIDGET_BASE] = {"Please place banknotes in the hopper and try again.", "请在进钞口放入纸钞后重试。", "호퍼에 지폐를 넣은 뒤 다시 시도하세요."},
    [UI_TEXT_WIDGET_FAULT_DIAGNOSTICS_TITLE - UI_TEXT_WIDGET_BASE] = {"MACHINE DIAGNOSTICS", "设备诊断", "장비 진단"},
    [UI_TEXT_WIDGET_FAULT_SELFTEST_ERROR - UI_TEXT_WIDGET_BASE] = {"SELF-TEST ERROR", "自检错误", "자가 점검 오류"},
    [UI_TEXT_WIDGET_FAULT_BOOT_SENSOR_FAILED - UI_TEXT_WIDGET_BASE] = {"Sensor Self-Test Failed", "传感器自检失败", "센서 자가 점검 실패"},
    [UI_TEXT_WIDGET_FAULT_BOOT_MOTOR_FAILED - UI_TEXT_WIDGET_BASE] = {"Motor Self-Test Failed", "电机自检失败", "모터 자가 점검 실패"},
    [UI_TEXT_WIDGET_FAULT_BOOT_MAGNET_FAILED - UI_TEXT_WIDGET_BASE] = {"Electromagnet Self-Test Failed", "电磁铁自检失败", "전자석 자가 점검 실패"},
    [UI_TEXT_WIDGET_FAULT_BOOT_CONFIG_FAILED - UI_TEXT_WIDGET_BASE] = {"Read Config Parameters Failed", "读取配置参数失败", "구성 파라미터 읽기 실패"},
    [UI_TEXT_WIDGET_FAULT_BOOT_IMAGE_FAILED - UI_TEXT_WIDGET_BASE] = {"Image Board Self-Test Failed", "图像板自检失败", "이미지 보드 자가 점검 실패"},
    [UI_TEXT_WIDGET_FAULT_BOOT_GENERIC_FAILED - UI_TEXT_WIDGET_BASE] = {"Boot Self-Test Failed", "开机自检失败", "부팅 자가 점검 실패"},
    [UI_TEXT_WIDGET_FAULT_RUNTIME_ERROR_SENSOR - UI_TEXT_WIDGET_BASE] = {"ERROR SENSOR", "运行错误传感器", "오류 센서"},
    [UI_TEXT_WIDGET_FAULT_RUNTIME_UNKNOWN - UI_TEXT_WIDGET_BASE] = {"Unknown Runtime Fault", "未知运行故障", "알 수 없는 런타임 고장"},
    [UI_TEXT_WIDGET_FAULT_REASON_TITLE - UI_TEXT_WIDGET_BASE] = {"Reason", "原因", "원인"},
    [UI_TEXT_WIDGET_FAULT_SOLUTION_TITLE - UI_TEXT_WIDGET_BASE] = {"Solution", "解决方案", "해결 방법"},
    [UI_TEXT_WIDGET_FAULT_CONFIRM - UI_TEXT_WIDGET_BASE] = {"CONFIRM", "确认", "확인"},
    [UI_TEXT_WIDGET_FAULT_TIME_FMT - UI_TEXT_WIDGET_BASE] = {"TIME: %s", "时间: %s", "시간: %s"},
    [UI_TEXT_WIDGET_FAULT_MODEL_FMT - UI_TEXT_WIDGET_BASE] = {"MODEL: %s", "机型: %s", "모델: %s"},
    [UI_TEXT_WIDGET_FAULT_REASON_FALLBACK - UI_TEXT_WIDGET_BASE] = {"Please check machine status and related hardware.", "请检查设备状态及相关硬件。", "장비 상태와 관련 하드웨어를 확인하세요."},
    [UI_TEXT_WIDGET_FAULT_SOLUTION_FALLBACK - UI_TEXT_WIDGET_BASE] = {"Press CONFIRM after checking the machine.", "检查设备后按确认。", "장비를 점검한 뒤 확인을 누르세요."}
};

static const char* ui_text_pick(const ui_text_item_t* item) //按当前语言选择文本
{
    if (item == NULL) {
        return "";
    }

    switch (ui_lang_get()) {
    case LANGUAGE_CN:
        return item->cn ? item->cn : item->en;

    case LANGUAGE_KR:
        return item->kr ? item->kr : item->en;

    case LANGUAGE_EN:
    default:
        return item->en ? item->en : "";
    }
}

const char* ui_text_get(ui_text_id_t text_id) //统一获取当前语言文本
{
    if (text_id < UI_TEXT_PAGE_MAX) {
        return ui_text_pick(&g_ui_text_page_group[text_id]);
    }

    if (text_id >= UI_TEXT_WIDGET_BASE && text_id < UI_TEXT_MAX) {
        return ui_text_pick(&g_ui_text_widget_group[text_id - UI_TEXT_WIDGET_BASE]);
    }

    return "";
}
