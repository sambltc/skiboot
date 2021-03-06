/* Copyright 2013-2014 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <skiboot.h>
#include <device.h>
#include <console.h>
#include <chip.h>
#include <ipmi.h>
#include <psi.h>
#include <npu-regs.h>

#include "astbmc.h"

#define NPU_BASE 0x8013c00
#define NPU_SIZE 0x2c
#define NPU_INDIRECT0	0x8000000008010c3f
#define NPU_INDIRECT1	0x8000000008010c7f

static void create_link(struct dt_node *npu, struct dt_node *pbcq, int index)
{
	struct dt_node *link;
	uint32_t lane_mask;
	uint64_t phy;
	char namebuf[32];

	snprintf(namebuf, sizeof(namebuf), "link@%x", index);
	link = dt_new(npu, namebuf);

	dt_add_property_string(link, "compatible", "ibm,npu-link");
	dt_add_property_cells(link, "ibm,npu-link-index", index);

	if (index < 4) {
		phy = NPU_INDIRECT0;
		lane_mask = 0xff << (index * 8);
	} else {
		phy = NPU_INDIRECT1;
		lane_mask = 0xff0000 >> (index - 3) * 8;
	}
	dt_add_property_u64s(link, "ibm,npu-phy", phy);
	dt_add_property_cells(link, "ibm,npu-lane-mask", lane_mask);
	dt_add_property_cells(link, "ibm,npu-pbcq", pbcq->phandle);
}

static void dt_create_npu(void)
{
        struct dt_node *xscom, *npu, *pbcq;
        char namebuf[32];

	dt_for_each_compatible(dt_root, xscom, "ibm,xscom") {
		snprintf(namebuf, sizeof(namebuf), "npu@%x", NPU_BASE);
		npu = dt_new(xscom, namebuf);
		dt_add_property_cells(npu, "reg", NPU_BASE, NPU_SIZE);
		dt_add_property_strings(npu, "compatible", "ibm,power8-npu");
		dt_add_property_cells(npu, "ibm,npu-index", 0);
		dt_add_property_cells(npu, "ibm,npu-links", 4);

		/* On Garrison we have 2 links per GPU device. The
		 * first 2 links go to the GPU connected via
		 * pbcq@2012c00 the second two via pbcq@2012800. */
		pbcq = dt_find_by_name(xscom, "pbcq@2012c00");
		assert(pbcq);
		create_link(npu, pbcq, 0);
		create_link(npu, pbcq, 1);
		pbcq = dt_find_by_name(xscom, "pbcq@2012800");
		assert(pbcq);
		create_link(npu, pbcq, 4);
		create_link(npu, pbcq, 5);
	}
}

static bool garrison_probe(void)
{
	if (!dt_node_is_compatible(dt_root, "ibm,garrison"))
		return false;

	/* Lot of common early inits here */
	astbmc_early_init();

	/*
	 * Override external interrupt policy -> send to Linux
	 *
	 * On Naples, we get LPC interrupts via the built-in LPC
	 * controller demuxer, not an external CPLD. The external
	 * interrupt is for other uses, such as the TPM chip, we
	 * currently route it to Linux, but we might change that
	 * later if we decide we need it.
	 */
	psi_set_external_irq_policy(EXTERNAL_IRQ_POLICY_LINUX);

	/* Fixups until HB get the NPU bindings */
	dt_create_npu();

	return true;
}

DECLARE_PLATFORM(garrison) = {
	.name			= "Garrison",
	.probe			= garrison_probe,
	.init			= astbmc_init,
	.cec_power_down         = astbmc_ipmi_power_down,
	.cec_reboot             = astbmc_ipmi_reboot,
	.elog_commit		= ipmi_elog_commit,
	.start_preload_resource	= flash_start_preload_resource,
	.resource_loaded	= flash_resource_loaded,
	.exit			= ipmi_wdt_final_reset,
	.terminate		= ipmi_terminate,
};
