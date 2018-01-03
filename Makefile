ARCH := armeabi-v7a
SDK_VERSION := 19

all: myall

myall:
	ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_ABI=$(ARCH) APP_PLATFORM=android-$(SDK_VERSION)
