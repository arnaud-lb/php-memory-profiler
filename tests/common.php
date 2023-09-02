<?php

// Some statement to prevent inclusion of this file from being optimized out
time();

function eat()
{
		return str_repeat('X', 3 * 1024 * 1024);
}

class Eater
{
		public static function eat()
		{
				return eat() . str_repeat('X', 5 * 1024 * 1024);
		}
}

