# Aurora Address Book Generator

## Generating get ent entries
python3 gen_ent_hostst.py filepath

contents of filepath should look like:
```
x4000c0s0b0n0.hsn.cm.aurora.alcf.anl.gov.
02:00:00:00:48:b2,9,2,50
```

## Generating address book
python3 gen_address_book.py filepath1 filepath2

contents of filepath1 should look like:
```
10.112.251.195  x4000c0s0b0n0-hsn0.hsn.cm.aurora.alcf.anl.gov
```

contents of filepath2 should look like:
```
neigh replace 10.112.251.196 dev HSNDEV lladdr 02:00:00:00:48:b2 nud permanent
```
