HSN CXI Address Calculation
===========================

Purpose
-------

This report summarizes the investigation into whether Copper transport
endpoints can be derived directly from HSN interface metadata on Aurora-class
systems.

The work used:

- a distributed collector that records hostnames, HSN IPv4 addresses, MAC
  addresses, and Thallium CXI self-addresses
- a parser and analyzer that compares those fields across cabinets, chassis,
  slots, and interface indices

Main Finding
------------

CXI addresses are structured, but they are not derivable from a simple
one-step mapping based only on HSN IPv4 address or MAC address.

The strongest observed structure is associated with:

- cabinet family
- chassis grouping
- slot placement
- interface-pair symmetry across ``cxi0`` through ``cxi7``

Questions Investigated
----------------------

The study was designed to answer the following questions:

1. Can a CXI address be predicted directly from an HSN IPv4 address?
2. Can a CXI address be predicted directly from the corresponding MAC address?
3. If not, what parts of the CXI hexadecimal representation exhibit stable
   structure?
4. Can a partial decoding rule be proposed from observed node, slot, and
   interface patterns?

Data Collection Summary
-----------------------

The collector produces rows with the following structure:

.. code-block:: text

   hostname | hsn0 | mac0 | cxi0 | hsn1 | mac1 | cxi1 | ... | hsn7 | mac7 | cxi7

The resulting table is suitable for:

- address-book validation
- endpoint provenance checks
- fabric regularity studies
- future tooling that reasons about network placement

Primary Conclusions
-------------------

- There is no strong direct equality rule between HSN last-octet values and
  CXI low-byte values.
- There is no strong direct equality rule between MAC tail bytes and CXI
  low-byte values.
- Higher CXI hexadecimal digits correlate with cabinet family.
- Middle hexadecimal digits show chassis-level structure.
- Interface-pair symmetry is visible across the CXI endpoint set.

Operational Use
---------------

This report is most useful for:

- validating address-book generation logic
- investigating inconsistent endpoint assignments
- checking whether a discovered address-book artifact looks structurally
  plausible

Local Source and Tooling
------------------------

The collector and related tooling live under:

- ``copper-tests/copper-get-addressbook/``

In particular:

- ``list_cxi_hsn_thallium.cpp``
- ``compile_thallium_addressbook.sh``
- ``run_thallium_addressbook.slurm``
- ``run_thallium_addressbook.pbs``
