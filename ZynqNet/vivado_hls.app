<project xmlns="com.autoesl.autopilot.project" name="ZynqNet" top="fpga_top" parsingAllHeaderFiles="true">
    <includePaths/>
    <libraryPaths/>
    <Simulation argv="">
        <SimFlow name="csim" optimizeCompile="true" ldflags="" mflags="" clean="true" csimMode="0" lastCsimMode="0"/>
    </Simulation>
    <files xmlns="">
        <file name="../../weights.bin" sc="0" tb="1" cflags=" "/>
        <file name="../../unittests.hpp" sc="0" tb="1" cflags=" "/>
        <file name="../../unittests.cpp" sc="0" tb="1" cflags=" "/>
        <file name="../../network.hpp" sc="0" tb="1" cflags=" "/>
        <file name="../../network.cpp" sc="0" tb="1" cflags=" "/>
        <file name="../../netconfig.hpp" sc="0" tb="1" cflags=" "/>
        <file name="../../netconfig.cpp" sc="0" tb="1" cflags=" "/>
        <file name="../../indata.bin" sc="0" tb="1" cflags=" "/>
        <file name="../../cpu_top.hpp" sc="0" tb="1" cflags=" "/>
        <file name="../../cpu_top.cpp" sc="0" tb="1" cflags=" "/>
        <file name="fpga_top.cpp" sc="0" tb="false" cflags=""/>
        <file name="fpga_top.hpp" sc="0" tb="false" cflags=""/>
        <file name="image_cache.cpp" sc="0" tb="false" cflags=""/>
        <file name="image_cache.hpp" sc="0" tb="false" cflags=""/>
        <file name="memory_controller.cpp" sc="0" tb="false" cflags=""/>
        <file name="memory_controller.hpp" sc="0" tb="false" cflags=""/>
        <file name="netconfig.hpp" sc="0" tb="false" cflags=""/>
        <file name="network.hpp" sc="0" tb="false" cflags=""/>
        <file name="output_cache.cpp" sc="0" tb="false" cflags=""/>
        <file name="output_cache.hpp" sc="0" tb="false" cflags=""/>
        <file name="processing_element.cpp" sc="0" tb="false" cflags=""/>
        <file name="processing_element.hpp" sc="0" tb="false" cflags=""/>
        <file name="weights_cache.cpp" sc="0" tb="false" cflags=""/>
        <file name="weights_cache.hpp" sc="0" tb="false" cflags=""/>
    </files>
    <solutions xmlns="">
        <solution name="zynq" status="active"/>
    </solutions>
</project>

