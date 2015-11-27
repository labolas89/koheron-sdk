
proc add_averager_module {module_name bram_addr_width} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -type clk                             clk
  create_bd_pin -dir I                                       tvalid
  create_bd_pin -dir I                                       restart
  create_bd_pin -dir I -from 31                        -to 0 din
  create_bd_pin -dir O -from 31                        -to 0 dout
  create_bd_pin -dir O                                       wen
  create_bd_pin -dir O -from 31                        -to 0 count

  set add_latency 3
  set sr_latency 1
  set fifo_rd_latency 1 

  # Create FIFO
  cell xilinx.com:ip:fifo_generator:13.0 fifo     \
    [list                                         \
      Input_Data_Width 32                         \
      Input_Depth      [expr 2**$bram_addr_width] \
      Data_Count       true                       \
      Data_Count_Width $bram_addr_width           \
      Reset_Pin        false]                     \
    [list                                         \
      clk clk                                     \
      dout dout]

  # Create Adder 
  cell xilinx.com:ip:c_addsub:12.0 adder \
    [list                                \
      A_Width.VALUE_SRC USER             \
      B_Width.VALUE_SRC USER             \
      A_Width           32               \
      B_Width           32               \
      Out_Width         32               \
      CE                false            \
      Latency           $add_latency     \
      Reset_Pin         false]           \
    [list                                \
      CLK clk                            \
      B   din                        \
      S   fifo/din]

  # Connect tvalid to FIFO write enable
  cell xilinx.com:ip:c_shift_ram:12.0 wen_shift_reg \
    [list                                           \
      Width.VALUE_SRC USER                          \
      Width 1                                       \
      Depth $add_latency]                           \
    [list                                           \
      CLK clk                                       \
      D   tvalid                                    \
      Q   fifo/wr_en]

  # Connect FIFO/dout to Adder (insert shift register)
  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg \
    [list                                       \
      Width.VALUE_SRC USER                      \
      Width 32                                  \
      Depth $sr_latency                         \
      SCLR true]                                \
    [list                                       \
      CLK clk                                   \
      Q adder/A                                 \
      D fifo/dout]

  # Enable reading FIFO once 
  # data_count == 2**$bram_addr_width - $add_latency - $sr_latency - fifo_rd_latency)

  set threshold_val [expr 2**$bram_addr_width-$add_latency-$sr_latency-$fifo_rd_latency]

  cell pavel-demin:user:comparator:1.0 comp \
    [list DATA_WIDTH $bram_addr_width]      \
    [list                                   \
      a       fifo/data_count               \
      a_geq_b fifo/rd_en]

  cell xilinx.com:ip:xlconstant:1.1 threshold \
    [list                                     \
      CONST_WIDTH $bram_addr_width            \
      CONST_VAL   $threshold_val]             \
    [list dout comp/b] 

  # Start counting once FIFO read enabled

  cell xilinx.com:ip:c_counter_binary:12.0 counter \
    [list                                          \
      Output_Width 32                              \
      CE true]                                     \
    [list                                          \
      CLK clk                                      \
      Q   count                                    \
      CE  comp/a_geq_b]

  current_bd_instance $bd

}
