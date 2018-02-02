set(src
	LightCullMain.cpp
)

add_executable(LightCull
	${src}
)

target_compile_definitions(LightCull PRIVATE
	RUSH_USING_NAMESPACE # Automatically use Rush namespace
)

target_link_libraries(LightCull 
	Rush
)
