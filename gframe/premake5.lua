local _includedirs=includedirs
if _ACTION=="xcode4" then
	_includedirs=sysincludedirs
end
local ygopro_config=function(static_core)
	kind "WindowedApp"
	cppdialect "C++14"
	rtti "Off"
	files { "**.cpp", "**.cc", "**.c", "**.h", "**.hpp" }
	excludes { "lzma/**", "sound_sdlmixer.*", "sound_irrklang.*", "irrklang_dynamic_loader.*", "sound_sfml.*", "sfAudio/**", "Android/**" }
	if _OPTIONS["oldwindows"] then
		files { "../overwrites/overwrites.cpp", "../overwrites/loader.asm" }
		filter "files:**.asm"
			exceptionhandling 'SEH'
		filter {}
	end

	defines "CURL_STATICLIB"
	if _OPTIONS["pics"] then
		defines { "DEFAULT_PIC_URL=" .. _OPTIONS["pics"] }
	end
	if _OPTIONS["fields"] then
		defines { "DEFAULT_FIELD_URL=" .. _OPTIONS["fields"] }
	end
	if _OPTIONS["covers"] then
		defines { "DEFAULT_COVER_URL=" .. _OPTIONS["covers"] }
	end
	if _OPTIONS["discord"] then
		defines { "DISCORD_APP_ID=" .. _OPTIONS["discord"] }
	end
	if _OPTIONS["update-url"] then
		defines { "UPDATE_URL=" .. _OPTIONS["update-url"] }
	end
	includedirs "../ocgcore"
	links { "clzma", "freetype", "Irrlicht" }
	filter "system:macosx or ios"
		links { "iconv" }
	filter {}
	if _OPTIONS["no-joystick"]=="false" then
		defines "YGOPRO_USE_JOYSTICK"
		filter { "system:not windows", "configurations:Debug" }
			links { "SDL2d" }
		filter { "system:not windows", "configurations:Release" }
			links { "SDL2" }
		filter "system:macosx"
			links { "CoreAudio.framework", "AudioToolbox.framework", "CoreVideo.framework", "ForceFeedback.framework", "Carbon.framework" }
		filter {}
	end
	if _OPTIONS["sound"] then
		if _OPTIONS["sound"]=="irrklang" then
			defines "YGOPRO_USE_IRRKLANG"
			_includedirs "../irrKlang/include"
			files "sound_irrklang.*"
			files "irrklang_dynamic_loader.*"
		end
		if _OPTIONS["sound"]=="sdl-mixer" then
			defines "YGOPRO_USE_SDL_MIXER"
			files "sound_sdlmixer.*"
			filter "system:windows"
				links { "version", "setupapi" }
			filter { "system:not windows", "configurations:Debug" }
				links { "SDL2d" }
			filter { "system:not windows", "configurations:Release" }
				links { "SDL2" }
			filter "system:not windows"
				links { "SDL2_mixer", "FLAC", "mpg123", "vorbisfile", "vorbis", "ogg" }
			filter "system:macosx"
				links { "CoreAudio.framework", "AudioToolbox.framework", "CoreVideo.framework", "ForceFeedback.framework", "Carbon.framework" }
		end
		if _OPTIONS["sound"]=="sfml" then
			defines "YGOPRO_USE_SFML"
			files "sound_sfml.*"
			_includedirs "../sfAudio/include"
			links { "sfAudio" }
			filter "system:not windows"
				links { "FLAC", "vorbisfile", "vorbis", "ogg", "openal" }
				if _OPTIONS["use-mpg123"] then
					links { "mpg123" }
				end
			filter "system:macosx or ios"
				links { "CoreAudio.framework", "AudioToolbox.framework" }
			filter { "system:windows", "action:not vs*" }
				links { "FLAC", "vorbisfile", "vorbis", "ogg", "OpenAL32" }
				if _OPTIONS["use-mpg123"] then
					links { "mpg123" }
				end
		end
	end
	
	filter {}
		_includedirs { "../freetype/include" }

	filter "system:windows"
		kind "ConsoleApp"
		files "ygopro.rc"
		_includedirs { "../irrlicht/include" }
		dofile("../irrlicht/defines.lua")

	filter { "system:windows", "action:vs*" }
		files "dpiawarescaling.manifest"

	filter { "system:windows", "options:no-direct3d" }
		defines "NO_IRR_COMPILE_WITH_DIRECT3D_9_"

	filter { "system:windows", "options:not no-direct3d" }
		defines "IRR_COMPILE_WITH_DX9_DEV_PACK"

	filter "system:not windows"
		defines "LUA_COMPAT_5_2"
		if _OPTIONS["discord"] then
			links "discord-rpc"
		end
		links { "sqlite3", "event", "event_pthreads", "dl", "git2" }

	filter { "system:windows", "action:not vs*" }
		if _OPTIONS["discord"] then
			links "discord-rpc"
		end
		links { "sqlite3", "event", "git2" }

	filter "system:macosx or ios"
		defines "LUA_USE_MACOSX"
		_includedirs { "/usr/local/include/irrlicht" }
		linkoptions { "-Wl,-rpath ./" }
		if os.istarget("macosx") then
			files { "*.m", "*.mm" }
			links { "curl", "Cocoa.framework", "IOKit.framework", "OpenGL.framework", "Security.framework" }
		else
			files { "iOS/**" }
			links { "UIKit.framework", "CoreMotion.framework", "OpenGLES.framework", "Foundation.framework", "QuartzCore.framework", "ssl", "crypto" }
		end
		if _OPTIONS["update-url"] then
			links "crypto"
		end
		if static_core then
			links "lua"
		end

	filter { "system:ios", "configurations:Release" }
		links "curl"
	filter { "system:ios", "configurations:Debug" }
		links "curl-d"

	filter { "system:macosx or ios", "configurations:Debug" }
		links "fmtd"

	filter { "system:macosx or ios", "configurations:Release" }
		links "fmt"

	filter { "system:linux or windows", "action:not vs*", "configurations:Debug" }
		if _OPTIONS["vcpkg-root"] then
			links { "png16d", "bz2d", "fmtd", "curl-d" }
		else
			links { "fmt", "curl" }
		end

	filter { "system:ios" }
		files { "ios-Info.plist" }
		xcodebuildsettings {
			["PRODUCT_BUNDLE_IDENTIFIER"] = "io.github.edo9300.ygopro" .. (static_core and "" or "dll")
		}

	filter { "system:linux or windows", "action:not vs*", "configurations:Release" }
		if _OPTIONS["vcpkg-root"] then
			links { "png", "bz2" }
		end
		links { "fmt", "curl" }

	filter "system:linux"
		defines "LUA_USE_LINUX"
		if _OPTIONS["vcpkg-root"] then
			_includedirs { _OPTIONS["vcpkg-root"] .. "/installed/x64-linux/include/irrlicht" }
		else
			_includedirs "/usr/include/irrlicht"
		end
		linkoptions { "-Wl,-rpath=./" }
		if static_core then
			links  "lua:static"
		end
		if _OPTIONS["vcpkg-root"] then
			links { "ssl", "crypto", "z", "jpeg" }
		end
		
		
	filter { "system:windows", "action:not vs*" }
		if static_core then
			links  "lua-c++"
		end
		if _OPTIONS["vcpkg-root"] then
			links { "ssl", "crypto", "z", "jpeg" }
		end

	filter "system:not windows"
		links { "pthread" }
	
	filter "system:windows"
		links { "opengl32", "ws2_32", "winmm", "gdi32", "kernel32", "user32", "imm32", "wldap32", "crypt32", "advapi32", "rpcrt4", "ole32", "uuid", "winhttp" }
end

include "lzma/."
if _OPTIONS["sound"]=="sfml" then
	include "../sfAudio"
end
project "ygopro"
	targetname "ygopro"
	if _OPTIONS["prebuilt-core"] then
		libdirs { _OPTIONS["prebuilt-core"] }
	end
	links { "ocgcore" }
	ygopro_config(true)

project "ygoprodll"
	targetname "ygoprodll"
	defines "YGOPRO_BUILD_DLL"
	ygopro_config()
