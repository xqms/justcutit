

find_program(INKSCAPE inkscape REQUIRED)

function(add_image outputlist image)
	add_custom_command(
		OUTPUT ${image}.png
		DEPENDS ${image}.svg
		COMMAND ${INKSCAPE} -z -f ${CMAKE_CURRENT_SOURCE_DIR}/${image}.svg
			--export-area-page --export-png ${CMAKE_CURRENT_BINARY_DIR}/${image}.png
	)
	set(${outputlist} ${${outputlist}} ${image}.png PARENT_SCOPE)
endfunction()

add_image(IMAGE_OUTPUTS cut-in)
add_image(IMAGE_OUTPUTS cut-out)

add_custom_target(icons ALL
	DEPENDS ${IMAGE_OUTPUTS}
)
