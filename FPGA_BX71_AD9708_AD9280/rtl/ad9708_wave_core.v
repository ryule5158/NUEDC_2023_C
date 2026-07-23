`timescale 1ns / 1ps

/* AD9708波形核心：完成DDS、任意波、幅度偏置处理和硬件扫频。 */
module ad9708_wave_core #(
    parameter RAM_ADDR_WIDTH = 10,       /* 波形RAM地址位宽。 */
    parameter DAC_MID_CODE = 8'd128      /* DAC零偏中心码。 */
)(
    input  wire                       cfg_clk,
    input  wire                       reset_n,
    input  wire                       dac_clk,

    input  wire                       cfg_commit_toggle,
    input  wire                       enable_cfg,
    input  wire [2:0]                 mode_cfg,
    input  wire [31:0]                freq_word_cfg,
    input  wire [31:0]                phase_offset_cfg,
    input  wire [7:0]                 constant_code_cfg,
    input  wire [10:0]                ram_points_cfg,
    input  wire [7:0]                 amplitude_cfg,
    input  wire [7:0]                 amplitude_fraction_cfg,
    input  wire [7:0]                 offset_cfg,
    input  wire [7:0]                 offset_fraction_cfg,
    input  wire                       phase_reset_toggle,

    input  wire                       sweep_enable_cfg,
    input  wire [1:0]                 sweep_mode_cfg,
    input  wire                       sweep_hold_cfg,
    input  wire                       sweep_direction_cfg,
    input  wire [31:0]                sweep_low_cfg,
    input  wire [31:0]                sweep_high_cfg,
    input  wire [31:0]                sweep_step_cfg,
    input  wire [31:0]                sweep_dwell_cfg,
    input  wire                       sweep_start_toggle,

    input  wire                       ram_wr_en,
    input  wire [RAM_ADDR_WIDTH-1:0]  ram_wr_addr,
    input  wire [7:0]                 ram_wr_data,

    (* IOB = "TRUE" *)
    output reg  [7:0]                 dac_data,
    output reg  [31:0]                phase_debug,
    output reg  [31:0]                current_freq_word,
    output reg                        sweep_running,
    output reg                        sweep_done,
    output reg                        sweep_direction_up
);

    /* 波形模式编码。 */
    localparam MODE_CONST  = 3'd0; /* 恒定码。 */
    localparam MODE_RAM    = 3'd1; /* 任意波RAM。 */
    localparam MODE_SAW    = 3'd2; /* 锯齿波。 */
    localparam MODE_TRI    = 3'd3; /* 三角波。 */
    localparam MODE_SQUARE = 3'd4; /* 方波。 */

    /* 线性扫频模式编码。 */
    localparam SWEEP_BIDIR  = 2'd0; /* 往返扫频。 */
    localparam SWEEP_LOOP   = 2'd1; /* 单向循环扫频。 */
    localparam SWEEP_ONCE   = 2'd2; /* 单次扫频。 */
    localparam SWEEP_MANUAL = 2'd3; /* 手动方向扫频。 */

    localparam RAM_DEPTH = (1 << RAM_ADDR_WIDTH); /* 波形RAM深度。 */

    /* STM32写、DAC时钟读的双时钟波形RAM。 */
    (* ram_style = "block" *) reg [7:0] wave_ram [0:RAM_DEPTH-1];
    reg [RAM_ADDR_WIDTH-1:0] ram_rd_addr;
    reg [7:0] ram_sample;

    /* 经提交翻转同步后一次性更新的DAC域配置。 */
    reg enable_dac;
    reg [2:0] mode_dac;
    reg [31:0] freq_word_dac;
    reg [31:0] phase_offset_dac;
    reg [7:0] constant_code_dac;
    reg [10:0] ram_points_dac;
    reg [7:0] amplitude_dac;
    reg [7:0] amplitude_fraction_dac;
    reg [7:0] offset_dac;
    reg [7:0] offset_fraction_dac;
    reg sweep_enable_dac;
    reg [1:0] sweep_mode_dac;
    reg sweep_hold_dac;
    reg sweep_direction_dac;
    reg [31:0] sweep_low_dac;
    reg [31:0] sweep_high_dac;
    reg [31:0] sweep_step_dac;
    reg [31:0] sweep_dwell_dac;

    /* 三个异步控制翻转量的同步寄存器。 */
    (* ASYNC_REG = "TRUE" *) reg [2:0] cfg_commit_sync;
    (* ASYNC_REG = "TRUE" *) reg [2:0] phase_reset_sync;
    (* ASYNC_REG = "TRUE" *) reg [3:0] sweep_start_sync;

    /* DDS相位、扫频驻留和当前原始采样。 */
    reg [31:0] phase_acc;
    reg [31:0] sweep_dwell_count;
    reg [7:0] selected_sample;

    /* RAM寻址与Q8.8幅度换算流水寄存器。 */
    reg [42:0] ram_phase_product_reg;
    reg [7:0] raw_sample_reg;
    (* use_dsp = "yes" *) reg [24:0] scaled_product_reg;
    reg signed [16:0] scaled_low_q_reg;
    reg [16:0] divided_product_q_reg;
    reg signed [16:0] divided_low_q_reg;
    reg [7:0] scaled_sample_reg;
    reg [7:0] division_error_reg;
    reg [7:0] code_quantization_error_reg;

    /* 同步翻转量转换出的单周期控制事件。 */
    wire cfg_commit_req = cfg_commit_sync[2] ^ cfg_commit_sync[1];
    wire phase_reset_req = phase_reset_sync[2] ^ phase_reset_sync[1];
    wire sweep_start_req = sweep_start_sync[3] ^ sweep_start_sync[2];

    /* 由DDS高位直接生成的基础波形采样。 */
    wire [7:0] saw_sample = phase_acc[31:24];
    wire [7:0] tri_sample = phase_acc[31] ?
                            ~phase_acc[30:23] : phase_acc[30:23];
    wire [7:0] square_sample = phase_acc[31] ? 8'hFF : 8'h00;

    /* Q8.8幅度A对应端点[offset-A, offset+A-1]。 */
    wire [15:0] amplitude_q = {amplitude_dac, amplitude_fraction_dac};
    wire [15:0] offset_q = {offset_dac, offset_fraction_dac};
    wire [16:0] amplitude_span_q = (amplitude_q == 16'd0) ? 17'd0 :
                                   ({1'b0, amplitude_q} << 1) - 17'd256;
    wire signed [16:0] scaled_low_q =
        $signed({1'b0, offset_q}) - $signed({1'b0, amplitude_q});

    /*
     * 将raw*span_q/255的余数反馈到下一采样，再对Q8.8结果误差反馈量化。
     * 24位数除以255可折叠三个字节，避免组合除法器。
     */
    wire [24:0] division_value =
        scaled_product_reg + {17'd0, division_error_reg};
    wire [7:0] division_byte2 = division_value[23:16];
    wire [7:0] division_byte1 = division_value[15:8];
    wire [7:0] division_byte0 = division_value[7:0];
    wire [9:0] division_fold = {2'd0, division_byte2} +
                               {2'd0, division_byte1} +
                               {2'd0, division_byte0};
    wire [1:0] division_fold_quotient =
        (division_fold >= 10'd510) ? 2'd2 :
        (division_fold >= 10'd255) ? 2'd1 : 2'd0;
    wire [9:0] division_fold_remainder =
        division_fold - ((division_fold_quotient == 2'd2) ? 10'd510 :
                         (division_fold_quotient == 2'd1) ? 10'd255 :
                         10'd0);
    wire [16:0] division_quotient =
        {1'b0, division_byte2, 8'd0} +
        {9'd0, division_byte2} +
        {9'd0, division_byte1} +
        {15'd0, division_fold_quotient};
    wire [7:0] division_remainder = division_fold_remainder[7:0];
    wire signed [17:0] interpolated_q =
        $signed({divided_low_q_reg[16], divided_low_q_reg}) +
        $signed({1'b0, divided_product_q_reg});
    wire signed [18:0] quantization_value =
        $signed({interpolated_q[17], interpolated_q}) +
        $signed({11'd0, code_quantization_error_reg});
    wire [7:0] limited_sample = (quantization_value < 19'sd0) ? 8'd0 :
                                (quantization_value > 19'sd65535) ? 8'd255 :
                                quantization_value[15:8];

    /* 仿真和综合共用的波形RAM初始化索引。 */
    integer init_i;
    /* 上电后将任意波RAM初始化为中点码。 */
    initial begin
        for (init_i = 0; init_i < RAM_DEPTH; init_i = init_i + 1) begin
            wave_ram[init_i] = DAC_MID_CODE;
        end
    end

    /* 根据当前模式选择满幅原始采样，随后统一执行幅度和偏置处理。 */
    always @(*) begin
        case (mode_dac)
            MODE_RAM:    selected_sample = ram_sample;
            MODE_SAW:    selected_sample = saw_sample;
            MODE_TRI:    selected_sample = tri_sample;
            MODE_SQUARE: selected_sample = square_sample;
            default:     selected_sample = constant_code_dac;
        endcase
    end

    /* 波形RAM写端口，写入期间应用层会先停止DAC输出。 */
    always @(posedge cfg_clk) begin
        if (ram_wr_en) begin
            wave_ram[ram_wr_addr] <= ram_wr_data;
        end
    end

    /* 同步控制翻转，并在提交事件到达时原子捕获全部多位配置。 */
    always @(posedge dac_clk or negedge reset_n) begin
        if (!reset_n) begin
            cfg_commit_sync <= 3'b000;
            phase_reset_sync <= 3'b000;
            sweep_start_sync <= 4'b0000;
            enable_dac <= 1'b0;
            mode_dac <= MODE_CONST;
            freq_word_dac <= 32'd0;
            phase_offset_dac <= 32'd0;
            constant_code_dac <= DAC_MID_CODE;
            ram_points_dac <= 11'd1024;
            amplitude_dac <= 8'd128;
            amplitude_fraction_dac <= 8'd0;
            offset_dac <= DAC_MID_CODE;
            offset_fraction_dac <= 8'd0;
            sweep_enable_dac <= 1'b0;
            sweep_mode_dac <= SWEEP_BIDIR;
            sweep_hold_dac <= 1'b0;
            sweep_direction_dac <= 1'b1;
            sweep_low_dac <= 32'd0;
            sweep_high_dac <= 32'd0;
            sweep_step_dac <= 32'd0;
            sweep_dwell_dac <= 32'd1;
        end else begin
            cfg_commit_sync <= {cfg_commit_sync[1:0], cfg_commit_toggle};
            phase_reset_sync <= {phase_reset_sync[1:0], phase_reset_toggle};
            sweep_start_sync <= {sweep_start_sync[2:0], sweep_start_toggle};

            if (cfg_commit_req) begin
                enable_dac <= enable_cfg;
                mode_dac <= mode_cfg;
                freq_word_dac <= freq_word_cfg;
                phase_offset_dac <= phase_offset_cfg;
                constant_code_dac <= constant_code_cfg;
                ram_points_dac <= ((ram_points_cfg >= 11'd2) &&
                                   (ram_points_cfg <= 11'd1024)) ?
                                  ram_points_cfg : 11'd1024;
                if ((amplitude_cfg == 8'd0) &&
                    (amplitude_fraction_cfg < 8'd128)) begin
                    amplitude_dac <= 8'd0;
                    amplitude_fraction_dac <= 8'd0;
                end else if (amplitude_cfg < 8'd128) begin
                    amplitude_dac <= amplitude_cfg;
                    amplitude_fraction_dac <= amplitude_fraction_cfg;
                end else begin
                    amplitude_dac <= 8'd128;
                    amplitude_fraction_dac <= 8'd0;
                end
                offset_dac <= offset_cfg;
                offset_fraction_dac <= offset_fraction_cfg;
                sweep_enable_dac <= sweep_enable_cfg;
                sweep_mode_dac <= sweep_mode_cfg;
                sweep_hold_dac <= sweep_hold_cfg;
                sweep_direction_dac <= sweep_direction_cfg;
                sweep_low_dac <= sweep_low_cfg;
                sweep_high_dac <= sweep_high_cfg;
                sweep_step_dac <= sweep_step_cfg;
                sweep_dwell_dac <= (sweep_dwell_cfg == 32'd0) ?
                                   32'd1 : sweep_dwell_cfg;
            end
        end
    end

    /* DDS、任意点数RAM寻址和硬件线性扫频状态机。 */
    always @(posedge dac_clk or negedge reset_n) begin
        if (!reset_n) begin
            phase_acc <= 32'd0;
            phase_debug <= 32'd0;
            ram_phase_product_reg <= 43'd0;
            ram_rd_addr <= {RAM_ADDR_WIDTH{1'b0}};
            ram_sample <= DAC_MID_CODE;
            raw_sample_reg <= DAC_MID_CODE;
            scaled_product_reg <= 25'd0;
            scaled_low_q_reg <= 17'sd0;
            divided_product_q_reg <= 17'd0;
            divided_low_q_reg <= 17'sd0;
            scaled_sample_reg <= DAC_MID_CODE;
            division_error_reg <= 8'd0;
            code_quantization_error_reg <= 8'd0;
            current_freq_word <= 32'd0;
            sweep_dwell_count <= 32'd0;
            sweep_running <= 1'b0;
            sweep_done <= 1'b0;
            sweep_direction_up <= 1'b1;
            dac_data <= DAC_MID_CODE;
        end else begin
            ram_phase_product_reg <= phase_acc * ram_points_dac;
            ram_rd_addr <= ram_phase_product_reg[41:32];
            ram_sample <= wave_ram[ram_rd_addr];
            raw_sample_reg <= selected_sample;
            scaled_product_reg <= raw_sample_reg * amplitude_span_q;
            scaled_low_q_reg <= scaled_low_q;
            divided_product_q_reg <= division_quotient;
            divided_low_q_reg <= scaled_low_q_reg;
            scaled_sample_reg <= limited_sample;

            if (!enable_dac || (mode_dac == MODE_CONST) ||
                (amplitude_q == 16'd0) || cfg_commit_req ||
                phase_reset_req) begin
                division_error_reg <= 8'd0;
            end else begin
                division_error_reg <= division_remainder;
            end

            if (!enable_dac || (mode_dac == MODE_CONST) ||
                cfg_commit_req || phase_reset_req) begin
                code_quantization_error_reg <= 8'd0;
            end else begin
                code_quantization_error_reg <= quantization_value[7:0];
            end

            if (!sweep_enable_dac) begin
                current_freq_word <= freq_word_dac;
                sweep_dwell_count <= 32'd0;
                sweep_running <= 1'b0;
                sweep_done <= 1'b0;
                sweep_direction_up <= 1'b1;
            end else if (sweep_start_req) begin
                sweep_direction_up <= (sweep_mode_dac == SWEEP_MANUAL) ?
                                      sweep_direction_dac : 1'b1;
                current_freq_word <= ((sweep_mode_dac == SWEEP_MANUAL) &&
                                      !sweep_direction_dac) ?
                                     sweep_high_dac : sweep_low_dac;
                sweep_dwell_count <= 32'd0;
                sweep_running <= 1'b1;
                sweep_done <= 1'b0;
            end else if (sweep_running && enable_dac && !sweep_hold_dac) begin
                if ((sweep_mode_dac == SWEEP_MANUAL) &&
                    (sweep_direction_up != sweep_direction_dac)) begin
                    sweep_direction_up <= sweep_direction_dac;
                    sweep_dwell_count <= 32'd0;
                    sweep_done <= 1'b0;
                end else if (sweep_dwell_count >= (sweep_dwell_dac - 1'b1)) begin
                    sweep_dwell_count <= 32'd0;

                    if (sweep_direction_up) begin
                        if ((current_freq_word >= sweep_high_dac) ||
                            (sweep_step_dac >=
                             (sweep_high_dac - current_freq_word))) begin
                            current_freq_word <= sweep_high_dac;
                            case (sweep_mode_dac)
                                SWEEP_BIDIR: begin
                                    sweep_direction_up <= 1'b0;
                                end
                                SWEEP_LOOP: begin
                                    current_freq_word <=
                                        (current_freq_word >= sweep_high_dac) ?
                                        sweep_low_dac : sweep_high_dac;
                                end
                                SWEEP_ONCE: begin
                                    sweep_running <= 1'b0;
                                    sweep_done <= 1'b1;
                                end
                                default: begin
                                    sweep_done <= 1'b1;
                                end
                            endcase
                        end else begin
                            current_freq_word <= current_freq_word + sweep_step_dac;
                            sweep_done <= 1'b0;
                        end
                    end else begin
                        if ((current_freq_word <= sweep_low_dac) ||
                            (sweep_step_dac >=
                             (current_freq_word - sweep_low_dac))) begin
                            current_freq_word <= sweep_low_dac;
                            if (sweep_mode_dac == SWEEP_BIDIR) begin
                                sweep_direction_up <= 1'b1;
                            end else begin
                                sweep_done <= 1'b1;
                            end
                        end else begin
                            current_freq_word <= current_freq_word - sweep_step_dac;
                            sweep_done <= 1'b0;
                        end
                    end
                end else begin
                    sweep_dwell_count <= sweep_dwell_count + 1'b1;
                end
            end

            if (phase_reset_req) begin
                phase_acc <= phase_offset_dac;
            end else if (enable_dac) begin
                phase_acc <= phase_acc + current_freq_word;
            end
            phase_debug <= phase_acc;

            if (!enable_dac) begin
                dac_data <= DAC_MID_CODE;
            end else if (mode_dac == MODE_CONST) begin
                dac_data <= constant_code_dac;
            end else begin
                dac_data <= scaled_sample_reg;
            end
        end
    end

endmodule
