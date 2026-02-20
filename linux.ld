SECTIONS
{
	.param ALIGN(8) :
	{
		PROVIDE(__start_param = .);
		KEEP(*(SORT_BY_NAME(param*)))
		. = ALIGN(8);
		PROVIDE(__stop_param = .);
	}
}
INSERT AFTER .rodata;
