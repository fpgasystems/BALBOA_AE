library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library work;
use work.rounds.all;

entity aes_top_new is
    generic(
        NPAR      : integer := 2;
        KEY_WIDTH : integer := 128;
        OPERATION : integer := 0;       -- [0-encryption, 1-decryption]
        MODE      : integer := 0        -- [0-ECB, 1-CTR, 2-CBC]
    );
    port(
        clk       : in std_logic;
        reset_n   : in std_logic; 
        stall     : in std_logic;
    -- key/Last/ keep
        key_in    : in  std_logic_vector(1408-1 downto 0);
        last_in   : in  std_logic;
        last_out  : out std_logic;
        keep_in   : in  std_logic_vector(NPAR*16-1 downto 0);
        keep_out  : out std_logic_vector(NPAR*16-1 downto 0);
    -- Data valid
        dVal_in   : in  std_logic; -- Data valid
        dVal_out  : out std_logic;
    -- Data
        data_in   : in  std_logic_vector(NPAR*128-1 downto 0); 
        data_out  : out std_logic_vector(NPAR*128-1 downto 0);
    -- CTR - nonce+value
        cntr_in   : in  std_logic_vector(NPAR*128-1 downto 0)
    );
end entity aes_top_new;

architecture RTL of aes_top_new is 
    type keep_array is array (NPAR-1 downto 0) of std_logic_vector(15 downto 0);
    
    type dataf_array is array (NPAR-1 downto 0) of std_logic_vector(NPAR*128-1 downto 0);
    type data_array is array (NPAR-1 downto 0) of std_logic_vector(128-1 downto 0);
    
    type keepf_array is array (NPAR-1 downto 0) of std_logic_vector(NPAR*16-1 downto 0);

    --variable key_rounds : integer := key_rounds_number(KEY_WIDTH);
    -- Internal signals
    signal dVal   : std_logic_vector(NPAR-1 downto 0);
    signal last   : std_logic_vector(NPAR-1 downto 0);

    signal s_cntr : std_logic_vector(NPAR*128-1 downto 0);

    signal keep   : keep_array;
    signal s_data : data_array;

    signal s_data_full : dataf_array;
    signal s_data_piped: std_logic_vector(NPAR*128-1 downto 0);
    signal s_keep_piped: keepf_array;
begin 

GEN_4: if(MODE=1) generate
    GEN_AES_PAR: for i in 0 to NPAR-1 generate
        GEN_AES_PIPE: entity work.aes_pipeline
            generic map(
                KEY_WIDTH  => KEY_WIDTH,
                KEY_ROUNDS => key_rounds_number(KEY_WIDTH),
                ENC_ROUNDS => encrypt_rounds_number(KEY_WIDTH),
                MODE  => MODE,
                OPERATION => OPERATION
            )
            port map(
                clk       => clk,
                reset_n   => reset_n,
                stall     => stall,
            -- Key/Last/Keep
                key_in    => key_in,
                last_in   => last_in,
                last_out  => last(i),
                keep_in   => keep_in(i*16+15 downto i*16),
                keep_out  => keep(i),
            -- Data valid
                dVal_in   => dVal_in,
                dVal_out  => dVal(i),
            -- Data
                data_in   => cntr_in((i+1)*128-1 downto i*128),
                data_out  => data_out(i*128+127 downto i*128),
            -- CTR
                cntr_in   => s_data_piped((i+1)*128-1 downto i*128)
            );
    end generate GEN_AES_PAR;

    GEN_DAT: if(MODE=1) generate 
        DATA_PIPE_0: entity work.data_pipeline
            generic map(
                NPAR       => NPAR,
                ENC_ROUNDS => encrypt_rounds_number(KEY_WIDTH)
            )
            port map(
                clk       => clk,
                reset_n   => reset_n, 
                stall     => stall,
            -- Key/Last/ keep
                keep_in   => keep_in,
                keep_out  => s_keep_piped(0),
                data_in  => data_in, 
                data_out => s_data_piped
            );
    end generate GEN_DAT; 

    GEN_VALID: process (dVal) is 
        variable tmp : std_logic;
        begin
            tmp := '0';
            for i in 0 to NPAR-1 loop
                tmp := tmp or dVal(i); 
            end loop;
            dVal_out <= tmp;
    end process GEN_VALID; 

    GEN_LAST: process (last) is 
        variable tmp : std_logic; 
        begin
            tmp := '0';
            for i in 0 to NPAR-1 loop
                tmp := tmp or last(i); 
            end loop;
            last_out <= tmp;
    end process GEN_LAST;

    GEN_KEEP: for i in 0 to NPAR-1 generate
        keep_out(i*16+15 downto i*16) <= keep(i);
    end generate GEN_KEEP;
end generate GEN_4;


end architecture RTL;