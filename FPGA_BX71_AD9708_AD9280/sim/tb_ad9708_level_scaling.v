`timescale 1ns / 1ps

module tb_ad9708_level_scaling;

    reg cfg_clk = 1'b0;
    reg dac_clk = 1'b0;
    reg reset_n = 1'b0;
    reg cfg_commit_toggle = 1'b0;
    reg enable_cfg = 1'b0;
    reg [2:0] mode_cfg = 3'd2;
    reg [31:0] freq_word_cfg = 32'h01000000;
    reg [31:0] phase_offset_cfg = 32'd0;
    reg [7:0] constant_code_cfg = 8'd128;
    reg [10:0] ram_points_cfg = 11'd1024;
    reg [7:0] amplitude_cfg = 8'd12;
    reg [7:0] amplitude_fraction_cfg = 8'd0;
    reg [7:0] offset_cfg = 8'd142;
    reg [7:0] offset_fraction_cfg = 8'd0;
    reg phase_reset_toggle = 1'b0;
    reg sweep_enable_cfg = 1'b0;
    reg [1:0] sweep_mode_cfg = 2'd0;
    reg sweep_hold_cfg = 1'b0;
    reg sweep_direction_cfg = 1'b1;
    reg [31:0] sweep_low_cfg = 32'd0;
    reg [31:0] sweep_high_cfg = 32'd0;
    reg [31:0] sweep_step_cfg = 32'd0;
    reg [31:0] sweep_dwell_cfg = 32'd1;
    reg sweep_start_toggle = 1'b0;
    reg ram_wr_en = 1'b0;
    reg [9:0] ram_wr_addr = 10'd0;
    reg [7:0] ram_wr_data = 8'd0;

    wire [7:0] dac_data;
    wire [31:0] phase_debug;
    wire [31:0] current_freq_word;
    wire sweep_running;
    wire sweep_done;
    wire sweep_direction_up;

    integer index;
    integer minimum_code;
    integer maximum_code;
    integer transition_count;
    integer previous_code;
    integer code_sum;

    always #10 cfg_clk = ~cfg_clk;
    always #4 dac_clk = ~dac_clk;

    ad9708_wave_core dut (
        .cfg_clk(cfg_clk),
        .reset_n(reset_n),
        .dac_clk(dac_clk),
        .cfg_commit_toggle(cfg_commit_toggle),
        .enable_cfg(enable_cfg),
        .mode_cfg(mode_cfg),
        .freq_word_cfg(freq_word_cfg),
        .phase_offset_cfg(phase_offset_cfg),
        .constant_code_cfg(constant_code_cfg),
        .ram_points_cfg(ram_points_cfg),
        .amplitude_cfg(amplitude_cfg),
        .amplitude_fraction_cfg(amplitude_fraction_cfg),
        .offset_cfg(offset_cfg),
        .offset_fraction_cfg(offset_fraction_cfg),
        .phase_reset_toggle(phase_reset_toggle),
        .sweep_enable_cfg(sweep_enable_cfg),
        .sweep_mode_cfg(sweep_mode_cfg),
        .sweep_hold_cfg(sweep_hold_cfg),
        .sweep_direction_cfg(sweep_direction_cfg),
        .sweep_low_cfg(sweep_low_cfg),
        .sweep_high_cfg(sweep_high_cfg),
        .sweep_step_cfg(sweep_step_cfg),
        .sweep_dwell_cfg(sweep_dwell_cfg),
        .sweep_start_toggle(sweep_start_toggle),
        .ram_wr_en(ram_wr_en),
        .ram_wr_addr(ram_wr_addr),
        .ram_wr_data(ram_wr_data),
        .dac_data(dac_data),
        .phase_debug(phase_debug),
        .current_freq_word(current_freq_word),
        .sweep_running(sweep_running),
        .sweep_done(sweep_done),
        .sweep_direction_up(sweep_direction_up)
    );

    /* 提交一组配置并等待其跨入125MHz DAC时钟域。 */
    task commit_configuration;
    begin
        @(posedge cfg_clk);
        cfg_commit_toggle = ~cfg_commit_toggle;
        repeat (12) @(posedge dac_clk);
    end
    endtask

    /* 统计连续512个DAC码的范围和变化次数。 */
    task measure_output;
    begin
        minimum_code = 255;
        maximum_code = 0;
        transition_count = 0;
        code_sum = 0;
        previous_code = dac_data;
        for (index = 0; index < 512; index = index + 1) begin
            @(posedge dac_clk);
            if (dac_data < minimum_code)
                minimum_code = dac_data;
            if (dac_data > maximum_code)
                maximum_code = dac_data;
            if (dac_data != previous_code)
                transition_count = transition_count + 1;
            code_sum = code_sum + dac_data;
            previous_code = dac_data;
        end
    end
    endtask

    initial begin
        repeat (5) @(posedge dac_clk);
        reset_n = 1'b1;

        commit_configuration();
        enable_cfg = 1'b1;
        commit_configuration();
        repeat (16) @(posedge dac_clk);
        measure_output();

        if ((minimum_code != 130) || (maximum_code != 153)) begin
            $display("TEST_FAIL: low-level range %0d..%0d", minimum_code,
                     maximum_code);
            $finish;
        end
        if (transition_count < 200) begin
            $display("TEST_FAIL: error feedback inactive, transitions=%0d",
                     transition_count);
            $finish;
        end

        enable_cfg = 1'b0;
        commit_configuration();
        amplitude_cfg = 8'd128;
        offset_cfg = 8'd128;
        commit_configuration();
        enable_cfg = 1'b1;
        commit_configuration();
        repeat (16) @(posedge dac_clk);
        measure_output();

        if ((minimum_code != 0) || (maximum_code != 255)) begin
            $display("TEST_FAIL: full-scale range %0d..%0d", minimum_code,
                     maximum_code);
            $finish;
        end

        enable_cfg = 1'b0;
        commit_configuration();
        mode_cfg = 3'd4;
        freq_word_cfg = 32'h40000000;
        amplitude_cfg = 8'd43;
        offset_cfg = 8'd136;
        offset_fraction_cfg = 8'd128;
        commit_configuration();
        enable_cfg = 1'b1;
        commit_configuration();
        repeat (16) @(posedge dac_clk);
        measure_output();

        if ((minimum_code != 93) || (maximum_code != 179)) begin
            $display("TEST_FAIL: fractional offset range %0d..%0d",
                     minimum_code, maximum_code);
            $finish;
        end
        if (code_sum != (136 * 512)) begin
            $display("TEST_FAIL: fractional offset mean sum=%0d", code_sum);
            $finish;
        end

        enable_cfg = 1'b0;
        commit_configuration();
        amplitude_cfg = 8'd42;
        amplitude_fraction_cfg = 8'd64;
        commit_configuration();
        enable_cfg = 1'b1;
        commit_configuration();
        repeat (16) @(posedge dac_clk);
        measure_output();

        if ((minimum_code != 94) || (maximum_code != 178)) begin
            $display("TEST_FAIL: fractional amplitude range %0d..%0d",
                     minimum_code, maximum_code);
            $finish;
        end
        if (code_sum != (136 * 512)) begin
            $display("TEST_FAIL: fractional amplitude mean sum=%0d", code_sum);
            $finish;
        end

        $display("TEST_PASS: Q8.8 amplitude and offset scaling");
        $finish;
    end

endmodule
