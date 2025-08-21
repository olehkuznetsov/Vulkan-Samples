TMP_DIR=/data/local/tmp/
SDCARD_DIR=/sdcard/Android/data/com.khronos.vulkan_samples/files

adb shell mkdir -p $TMP_DIR
adb shell mkdir -p $SDCARD_DIR

adb push --sync shaders $TMP_DIR
adb shell cp -r /data/local/tmp/shaders $SDCARD_DIR

adb push --sync assets  $TMP_DIR/assets
adb shell cp -r $TMP_DIR/assets/ $SDCARD_DIR

adb shell chmod -R 777 $SDCARD_DIR
