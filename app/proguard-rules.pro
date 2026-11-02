#-dontshrink
#-keepclasseswithmembernames class com.nvidia.devtech.*, com.wardrumstudios.utils.*

-keep class com.nvidia.devtech.* { *; }

-keep class com.wardrumstudios.utils.* { *; }

-keep class com.samp.mobile.game.* { *; }

-dontwarn javax.servlet.**
-dontwarn org.conscrypt.**
-dontwarn org.bouncycastle.**
-dontwarn org.openjsse.**

# GTA:SA 2.11's engine binds to these by class, method and field name
# (docs/OSWRAPPER-2.11.md).
-keep class com.rockstargames.oswrapper.** { *; }
