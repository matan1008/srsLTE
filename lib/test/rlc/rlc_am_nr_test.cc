/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "rlc_test_common.h"
#include "srsran/common/buffer_pool.h"
#include "srsran/common/rlc_pcap.h"
#include "srsran/common/test_common.h"
#include "srsran/common/threads.h"
#include "srsran/interfaces/ue_pdcp_interfaces.h"
#include "srsran/interfaces/ue_rrc_interfaces.h"
#include "srsran/rlc/rlc_am_nr.h"

#define NBUFS 5
#define HAVE_PCAP 0
#define SDU_SIZE 500

using namespace srsue;
using namespace srsran;

int basic_test_tx(rlc_am* rlc, byte_buffer_t pdu_bufs[NBUFS])
{
  // Push 5 SDUs into RLC1
  unique_byte_buffer_t sdu_bufs[NBUFS];
  for (int i = 0; i < NBUFS; i++) {
    sdu_bufs[i]             = srsran::make_byte_buffer();
    sdu_bufs[i]->msg[0]     = i; // Write the index into the buffer
    sdu_bufs[i]->N_bytes    = 1; // Give each buffer a size of 1 byte
    sdu_bufs[i]->md.pdcp_sn = i; // PDCP SN for notifications
    rlc->write_sdu(std::move(sdu_bufs[i]));
  }

  TESTASSERT(15 == rlc->get_buffer_state()); // 2 Bytes * NBUFFS (header size) + NBUFFS (data) = 15

  // Read 5 PDUs from RLC1 (1 byte each)
  for (int i = 0; i < NBUFS; i++) {
    uint32_t len        = rlc->read_pdu(pdu_bufs[i].msg, 3); // 2 bytes for header + 1 byte payload
    pdu_bufs[i].N_bytes = len;
    TESTASSERT(3 == len);
  }

  TESTASSERT(0 == rlc->get_buffer_state());
  return SRSRAN_SUCCESS;
}

int basic_test()
{
  rlc_am_tester tester;
  timer_handler timers(8);
  byte_buffer_t pdu_bufs[NBUFS];

  rlc_am rlc1(srsran_rat_t::nr, srslog::fetch_basic_logger("RLC_AM_1"), 1, &tester, &tester, &timers);
  rlc_am rlc2(srsran_rat_t::nr, srslog::fetch_basic_logger("RLC_AM_2"), 1, &tester, &tester, &timers);

  // before configuring entity
  TESTASSERT(0 == rlc1.get_buffer_state());

  if (not rlc1.configure(rlc_config_t::default_rlc_am_config())) {
    return -1;
  }

  if (not rlc2.configure(rlc_config_t::default_rlc_am_config())) {
    return -1;
  }

  basic_test_tx(&rlc1, pdu_bufs);

  // Write 5 PDUs into RLC2
  for (int i = 0; i < NBUFS; i++) {
    rlc2.write_pdu(pdu_bufs[i].msg, pdu_bufs[i].N_bytes);
  }

  TESTASSERT(3 == rlc2.get_buffer_state());
  // Read status PDU from RLC2
  byte_buffer_t status_buf;
  int           len  = rlc2.read_pdu(status_buf.msg, 3);
  status_buf.N_bytes = len;

  // TESTASSERT(0 == rlc2.get_buffer_state());

  // Assert status is correct
  rlc_am_nr_status_pdu_t status_check = {};
  rlc_am_nr_read_status_pdu(&status_buf, rlc_am_nr_sn_size_t::size12bits, &status_check);
  TESTASSERT(status_check.ack_sn == 5); // 5 is the last SN that was not received.
  // TESTASSERT(rlc_am_is_valid_status_pdu(status_check));
  /*
        // Write status PDU to RLC1
        rlc1.write_pdu(status_buf.msg, status_buf.N_bytes);

        // Check PDCP notifications
        TESTASSERT(tester.notified_counts.size() == 5);
        for (uint16_t i = 0; i < tester.sdus.size(); i++) {
          TESTASSERT(tester.sdus[i]->N_bytes == 1);
          TESTASSERT(*(tester.sdus[i]->msg) == i);
          TESTASSERT(tester.notified_counts[i] == 1);
        }

        // Check statistics
        TESTASSERT(rx_is_tx(rlc1.get_metrics(), rlc2.get_metrics()));
      */
  return SRSRAN_SUCCESS;
}

int main(int argc, char** argv)
{
  // Setup the log message spy to intercept error and warning log entries from RLC
  if (!srslog::install_custom_sink(srsran::log_sink_message_spy::name(),
                                   std::unique_ptr<srsran::log_sink_message_spy>(
                                       new srsran::log_sink_message_spy(srslog::get_default_log_formatter())))) {
    return SRSRAN_ERROR;
  }

  auto* spy = static_cast<srsran::log_sink_message_spy*>(srslog::find_sink(srsran::log_sink_message_spy::name()));
  if (spy == nullptr) {
    return SRSRAN_ERROR;
  }
  srslog::set_default_sink(*spy);

  auto& logger_rlc1 = srslog::fetch_basic_logger("RLC_NR_AM_1", *spy, false);
  auto& logger_rlc2 = srslog::fetch_basic_logger("RLC_NR_AM_2", *spy, false);
  logger_rlc1.set_hex_dump_max_size(100);
  logger_rlc2.set_hex_dump_max_size(100);
  logger_rlc1.set_level(srslog::basic_levels::debug);
  logger_rlc2.set_level(srslog::basic_levels::debug);

  // start log backend
  srslog::init();

  TESTASSERT(basic_test() == SRSRAN_SUCCESS);

  return SRSRAN_SUCCESS;
}
