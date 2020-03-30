<?php

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

