// license:BSD-3-Clause
// copyright-holders:AJR
/****************************************************************************

    Skeleton driver for Ampro Little Board/PC.

    This is unusual among PC/XT-compatible machines in that many standard
    peripheral functions, including the interrupt and refresh controllers,
    are integrated into the V40 CPU itself, with some software assistance
    to compensate for DMAC incompatibilities. Two Vadem SDIP64 ASICs and a
    standard FDC and UART provide most other PC-like hardware features. The
    BIOS also supports the onboard SCSI controller.

****************************************************************************/

#include "emu.h"
#include "bus/isa/isa.h"
#include "bus/isa/isa_cards.h"
#include "bus/nscsi/devices.h"
#include "bus/rs232/rs232.h"
#include "cpu/nec/v5x.h"
#include "machine/ins8250.h"
#include "machine/ncr5380n.h"
#include "machine/upd765.h"
#include "sound/spkrdev.h"
#include "speaker.h"

class lbpc_state : public driver_device
{
public:
	lbpc_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_expbus(*this, "expbus")
		, m_speaker(*this, "speaker")
		, m_port61(0xff)
		, m_speaker_data(false)
	{
	}

	void lbpc(machine_config &config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	u8 exp_dack1_r();
	void exp_dack1_w(u8 data);
	DECLARE_WRITE_LINE_MEMBER(iochck_w);
	u8 port61_r();
	void port61_w(u8 data);
	DECLARE_WRITE_LINE_MEMBER(out2_w);

	void mem_map(address_map &map);
	void io_map(address_map &map);

	required_device<v40_device> m_maincpu;
	required_device<isa8_device> m_expbus;
	required_device<speaker_sound_device> m_speaker;

	u8 m_port61;
	bool m_speaker_data;
};


void lbpc_state::machine_start()
{
	save_item(NAME(m_port61));
	save_item(NAME(m_speaker_data));
}

void lbpc_state::machine_reset()
{
	port61_w(0);
}


u8 lbpc_state::exp_dack1_r()
{
	return m_expbus->dack_r(0);
}

void lbpc_state::exp_dack1_w(u8 data)
{
	m_expbus->dack_w(0, data);
}

WRITE_LINE_MEMBER(lbpc_state::iochck_w)
{
	// TODO
}

u8 lbpc_state::port61_r()
{
	return m_port61;
}

void lbpc_state::port61_w(u8 data)
{
	if (BIT(m_port61, 1) && !BIT(data, 1))
		m_speaker->level_w(0);
	else if (!BIT(m_port61, 1) && BIT(data, 1))
		m_speaker->level_w(m_speaker_data);
	m_maincpu->tctl2_w(BIT(data, 0));

	m_port61 = data;
}

WRITE_LINE_MEMBER(lbpc_state::out2_w)
{
	m_speaker_data = state;
	if (BIT(m_port61, 1))
		m_speaker->level_w(state);
}

void lbpc_state::mem_map(address_map &map)
{
	map(0x00000, 0x9ffff).ram(); // 256K, 512K or 768K DRAM
	// 0xE0000–0xEFFFF: empty socket
	// 0xF0000-0xF7FFF: empty socket
	map(0xf8000, 0xfffff).rom().region("bios", 0);
}

void lbpc_state::io_map(address_map &map)
{
	map(0x0330, 0x0337).rw("scsi:7:ncr", FUNC(ncr53c80_device::read), FUNC(ncr53c80_device::write));
	map(0x0370, 0x0377).m("fdc", FUNC(wd37c65c_device::map));
	map(0x03f8, 0x03ff).rw("com", FUNC(ins8250_device::ins8250_r), FUNC(ins8250_device::ins8250_w));
}


static INPUT_PORTS_START(lbpc)
INPUT_PORTS_END


void lbpc_state::lbpc(machine_config &config)
{
	V40(config, m_maincpu, 14.318181_MHz_XTAL); // 7.16 MHz operating frequency
	m_maincpu->set_addrmap(AS_PROGRAM, &lbpc_state::mem_map);
	m_maincpu->set_addrmap(AS_IO, &lbpc_state::io_map);
	m_maincpu->set_tclk(14.318181_MHz_XTAL / 12); // generated by ASIC1
	m_maincpu->out_handler<2>().set(FUNC(lbpc_state::out2_w));
	m_maincpu->out_hreq_cb().set_inputline(m_maincpu, INPUT_LINE_HALT);
	m_maincpu->out_hreq_cb().append(m_maincpu, FUNC(v40_device::hack_w));
	m_maincpu->in_memr_cb().set([this] (offs_t offset) { return m_maincpu->space(AS_PROGRAM).read_byte(offset); });
	m_maincpu->out_memw_cb().set([this] (offs_t offset, u8 data) { m_maincpu->space(AS_PROGRAM).write_byte(offset, data); });
	m_maincpu->in_ior_cb<0>().set(FUNC(lbpc_state::exp_dack1_r));
	m_maincpu->out_iow_cb<0>().set(FUNC(lbpc_state::exp_dack1_w));
	m_maincpu->in_ior_cb<1>().set("fdc", FUNC(wd37c65c_device::dma_r));
	m_maincpu->out_iow_cb<1>().set("fdc", FUNC(wd37c65c_device::dma_w));
	m_maincpu->in_ior_cb<2>().set("scsi:7:ncr", FUNC(ncr53c80_device::dma_r));
	m_maincpu->out_iow_cb<2>().set("scsi:7:ncr", FUNC(ncr53c80_device::dma_w));

	SPEAKER(config, "mono").front_center();
	SPEAKER_SOUND(config, m_speaker).add_route(ALL_OUTPUTS, "mono", 0.5);

	ins8250_device &com(INS8250(config, "com", 1.8432_MHz_XTAL)); // NS8250AV
	com.out_int_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ4);
	com.out_rts_callback().set("serial", FUNC(rs232_port_device::write_rts)); // J3 pin 4
	com.out_tx_callback().set("serial", FUNC(rs232_port_device::write_txd)); // J3 pin 5
	com.out_dtr_callback().set("serial", FUNC(rs232_port_device::write_dtr)); // J3 pin 7

	wd37c65c_device &fdc(WD37C65C(config, "fdc", 16_MHz_XTAL, 9.6_MHz_XTAL)); // WD37C65BJM
	fdc.intrq_wr_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ6);
	fdc.drq_wr_callback().set(m_maincpu, FUNC(v40_device::dreq_w<1>));

	NSCSI_BUS(config, "scsi");
	NSCSI_CONNECTOR(config, "scsi:0", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:1", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:2", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:3", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:4", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:5", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:6", default_scsi_devices, nullptr);
	NSCSI_CONNECTOR(config, "scsi:7").option_set("ncr", NCR53C80).machine_config([this] (device_t *device) {
		downcast<ncr5380n_device &>(*device).drq_handler().set(m_maincpu, FUNC(v40_device::dreq_w<2>));
	});

	rs232_port_device &serial(RS232_PORT(config, "serial", default_rs232_devices, nullptr));
	serial.dcd_handler().set("com", FUNC(ins8250_device::dcd_w)); // J3 pin 1
	serial.dsr_handler().set("com", FUNC(ins8250_device::dsr_w)); // J3 pin 2
	serial.rxd_handler().set("com", FUNC(ins8250_device::rx_w)); // J3 pin 3
	serial.cts_handler().set("com", FUNC(ins8250_device::cts_w)); // J3 pin 6
	serial.ri_handler().set("com", FUNC(ins8250_device::ri_w)); // J3 pin 8

	ISA8(config, m_expbus, 14.318181_MHz_XTAL / 2);
	m_expbus->set_memspace(m_maincpu, AS_PROGRAM);
	m_expbus->set_iospace(m_maincpu, AS_IO);
	m_expbus->drq1_callback().set(m_maincpu, FUNC(v40_device::dreq_w<0>));
	m_expbus->irq2_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ2);
	m_expbus->irq3_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ3);
	m_expbus->irq5_callback().set_inputline(m_maincpu, INPUT_LINE_IRQ5);
	m_expbus->iochck_callback().set(FUNC(lbpc_state::iochck_w));

	ISA8_SLOT(config, "exp", 0, m_expbus, pc_isa8_cards, "ega", false);
}


ROM_START(lbpc)
	ROM_REGION(0x8000, "bios", 0)
	// "Firmware Version 1.0H  03/08/89"
	ROM_LOAD("lbpc-bio.rom", 0x0000, 0x8000, CRC(47bddf8b) SHA1(8a04fe34502f9f3bfe1e233762bbd5bbdd1c455d))
ROM_END


COMP(1989, lbpc, 0, 0, lbpc, lbpc, lbpc_state, empty_init, "Ampro Computers", "Little Board/PC", MACHINE_IS_SKELETON)
