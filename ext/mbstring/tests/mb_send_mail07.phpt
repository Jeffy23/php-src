--TEST--
mb_send_mail() test 7 (lang=Korean)
--SKIPIF--
<?php
if (@mb_send_mail() === false || !mb_language("Korean")) {
	die("skip mb_send_mail() not available");
}
if (!@mb_internal_encoding('ISO-2022-KR')) {
	die("skip ISO-2022-KR encoding is not avaliable on this platform");
}
?>
--INI--
sendmail_path=cat
--FILE--
<?php
$to = 'example@example.com';

/* default setting */
mb_send_mail($to, mb_language(), "test");

/* Korean */
if (mb_language("korean")) {
	mb_internal_encoding('EUC-KR');
	mb_send_mail($to, "테스트 ".mb_language(), "테스트");
}
?>
--EXPECTF--
To: example@example.com
Subject: %s
Mime-Version: 1.0
Content-Type: text/plain; charset=%s
Content-Transfer-Encoding: %s

%s
To: example@example.com
Subject: =?ISO-2022-KR?B?GyQpQw5FVz06Ri4PIEtvcmVhbg8=?=
Mime-Version: 1.0
Content-Type: text/plain; charset=ISO-2022-KR
Content-Transfer-Encoding: 7bit

$)CEW=:F.
